#include "recovery_manager.h"

#include <fstream>
#include <iostream>
#include <algorithm>

RecoveryManager::RecoveryManager(
    std::shared_ptr<Store> store,
    const std::string &data_file,
    const std::string &wal_file)
    : store_(store),
      data_file_(data_file),
      wal_file_(wal_file)
{
}

// ---------------- helpers ----------------

uint64_t RecoveryManager::readU64(const char *p)
{
    uint64_t v = 0;
    for (int i = 0; i < 8; i++)
        v |= (uint64_t(uint8_t(p[i])) << (8 * i));
    return v;
}

uint32_t RecoveryManager::readU32(const char *p)
{
    uint32_t v = 0;
    for (int i = 0; i < 4; i++)
        v |= (uint32_t(uint8_t(p[i])) << (8 * i));
    return v;
}

// ---------------- snapshot load ----------------

void RecoveryManager::loadSnapshot()
{
    std::ifstream file(data_file_, std::ios::binary);

    if (!file.is_open())
    {
        std::cout << "[recovery] no snapshot found\n";
        return;
    }

    uint32_t magic;
    uint32_t version;
    uint64_t checkpoint_lsn;
    uint64_t count;

    file.read(reinterpret_cast<char *>(&magic), 4);
    file.read(reinterpret_cast<char *>(&version), 4);
    file.read(reinterpret_cast<char *>(&checkpoint_lsn), 8);
    file.read(reinterpret_cast<char *>(&count), 8);

    if (!file || magic != 0x4B564442)
    {
        std::cout << "[recovery] invalid snapshot\n";
        return;
    }

    checkpoint_lsn_ = checkpoint_lsn;

    for (uint64_t i = 0; i < count; i++)
    {
        uint16_t klen, vlen;

        file.read(reinterpret_cast<char *>(&klen), 2);
        std::string key(klen, '\0');
        file.read(&key[0], klen);

        file.read(reinterpret_cast<char *>(&vlen), 2);
        std::string value(vlen, '\0');
        file.read(&value[0], vlen);

        if (!file)
        {
            std::cout << "[recovery] corrupted snapshot\n";
            return;
        }

        store_->put(key, value);
    }

    std::cout << "[recovery] snapshot loaded, checkpoint_lsn="
              << checkpoint_lsn_
              << " entries=" << count << "\n";
}

// ---------------- WAL recovery ----------------

void RecoveryManager::recover()
{
    /*
        STEP 1: load snapshot
    */
    loadSnapshot();

    /*
        STEP 2: replay WAL
    */
    std::ifstream file(wal_file_, std::ios::binary);

    if (!file.is_open())
    {
        std::cout << "[recovery] no WAL found\n";
        return;
    }

    std::unordered_map<TxnId, TxnState> txns;
    std::vector<std::pair<TxnId, uint64_t>> commit_order;

    while (true)
    {
        uint32_t crc;
        if (!file.read(reinterpret_cast<char *>(&crc), 4))
            break;

        uint64_t lsn;
        uint32_t txn_id;
        uint8_t type;

        file.read(reinterpret_cast<char *>(&lsn), 8);
        file.read(reinterpret_cast<char *>(&txn_id), 4);
        file.read(reinterpret_cast<char *>(&type), 1);

        if (!file)
            break;

        Key key;
        Value value;

        /*
            IMPORTANT: payload depends on type
        */

        if (type == static_cast<uint8_t>(RecordType::PUT))
        {
            uint16_t klen, vlen;

            file.read(reinterpret_cast<char *>(&klen), 2);
            key.resize(klen);
            file.read(&key[0], klen);

            file.read(reinterpret_cast<char *>(&vlen), 2);
            value.resize(vlen);
            file.read(&value[0], vlen);
        }
        else if (type == static_cast<uint8_t>(RecordType::DELETE))
        {
            uint16_t klen;

            file.read(reinterpret_cast<char *>(&klen), 2);
            key.resize(klen);
            file.read(&key[0], klen);
        }
        else
        {
            // BEGIN / COMMIT / ABORT → no payload
        }

        if (!file)
            break;

        /*
            rebuild body for CRC validation
        */
        std::string body;

        body.append(reinterpret_cast<char *>(&lsn), 8);
        body.append(reinterpret_cast<char *>(&txn_id), 4);
        body.push_back(static_cast<char>(type));

        if (type == static_cast<uint8_t>(RecordType::PUT))
        {
            uint16_t klen = key.size();
            uint16_t vlen = value.size();

            body.append(reinterpret_cast<char *>(&klen), 2);
            body.append(key);

            body.append(reinterpret_cast<char *>(&vlen), 2);
            body.append(value);
        }
        else if (type == static_cast<uint8_t>(RecordType::DELETE))
        {
            uint16_t klen = key.size();

            body.append(reinterpret_cast<char *>(&klen), 2);
            body.append(key);
        }

        if (crc != crc32(body))
        {
            std::cout << "[recovery] CRC mismatch, stopping\n";
            break;
        }

        /*
            skip already checkpointed
        */
        if (lsn <= checkpoint_lsn_)
            continue;

        auto &txn = txns[txn_id];

        if (type == static_cast<uint8_t>(RecordType::PUT))
        {
            txn.ops.push_back({Operation::Type::PUT, key, value});
        }
        else if (type == static_cast<uint8_t>(RecordType::DELETE))
        {
            txn.ops.push_back({Operation::Type::DELETE, key, ""});
        }
        else if (type == static_cast<uint8_t>(RecordType::COMMIT))
        {
            txn.committed = true;
            txn.commit_lsn = lsn;
            commit_order.push_back({txn_id, lsn});
        }
        else if (type == static_cast<uint8_t>(RecordType::ABORT))
        {
            txn.committed = false;
        }
    }

    /*
        STEP 3: apply committed transactions in order
    */
    std::sort(commit_order.begin(), commit_order.end(),
              [](auto &a, auto &b)
              {
                  return a.second < b.second;
              });

    for (auto &entry : commit_order)
    {
        TxnId tid = entry.first;
        auto &txn = txns[tid];

        if (!txn.committed)
            continue;

        for (auto &op : txn.ops)
        {
            if (op.type == Operation::Type::PUT)
                store_->put(op.key, op.value);
            else
                store_->delete_key(op.key);
        }
    }

    std::cout << "[recovery] WAL replay done, committed txns="
              << commit_order.size() << "\n";
}

// ---------------- CRC ----------------

uint32_t RecoveryManager::crc32(const std::string &data)
{
    uint32_t crc = 0xFFFFFFFF;

    for (unsigned char c : data)
    {
        crc ^= c;
        for (int i = 0; i < 8; i++)
            crc = (crc & 1) ? (crc >> 1) ^ 0xEDB88320 : (crc >> 1);
    }

    return ~crc;
}
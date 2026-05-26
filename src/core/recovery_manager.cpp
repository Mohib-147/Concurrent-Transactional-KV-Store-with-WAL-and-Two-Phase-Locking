#include "recovery_manager.h"
#include <fstream>
#include <iostream>
#include <algorithm>

RecoveryManager::RecoveryManager(std::shared_ptr<Store> store,
                                 const std::string &wal_file)
    : store_(store), wal_file_(wal_file) {}

uint64_t RecoveryManager::readU64(const char *data)
{
    uint64_t v = 0;
    for (int i = 0; i < 8; i++)
        v |= (uint64_t(uint8_t(data[i])) << (8 * i));
    return v;
}

uint32_t RecoveryManager::readU32(const char *data)
{
    uint32_t v = 0;
    for (int i = 0; i < 4; i++)
        v |= (uint32_t(uint8_t(data[i])) << (8 * i));
    return v;
}

void RecoveryManager::recover()
{
    std::ifstream file(wal_file_, std::ios::binary);

    if (!file.is_open())
        return;

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

        uint16_t klen, vlen;

        file.read(reinterpret_cast<char *>(&klen), 2);
        std::string key(klen, '\0');
        file.read(&key[0], klen);

        file.read(reinterpret_cast<char *>(&vlen), 2);
        std::string value(vlen, '\0');
        file.read(&value[0], vlen);

        std::string body;
        body.append(reinterpret_cast<char *>(&lsn), 8);
        body.append(reinterpret_cast<char *>(&txn_id), 4);
        body.push_back((char)type);
        body.append(reinterpret_cast<char *>(&klen), 2);
        body.append(key);
        body.append(reinterpret_cast<char *>(&vlen), 2);
        body.append(value);

        if (crc != crc32(body))
        {
            std::cout << "CRC mismatch - stopping recovery\n";
            break;
        }

        auto &txn = txns[txn_id];

        if (type == (uint8_t)RecordType::PUT)
        {
            txn.ops.push_back({Operation::Type::PUT, key, value});
        }
        else if (type == (uint8_t)RecordType::DELETE)
        {
            txn.ops.push_back({Operation::Type::DELETE, key, ""});
        }
        else if (type == (uint8_t)RecordType::COMMIT)
        {
            txn.committed = true;
            txn.commit_lsn = lsn;
            commit_order.push_back({txn_id, lsn});
        }
        else if (type == (uint8_t)RecordType::ABORT)
        {
            txn.committed = false;
        }
    }

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

    std::cout << "Recovery complete. Txns applied: "
              << commit_order.size() << std::endl;
}

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
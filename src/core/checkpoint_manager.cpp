#include "checkpoint_manager.h"

#include <fstream>
#include <filesystem>
#include <cstdio>
#include <fcntl.h>
#include <unistd.h>
#include <sstream>

CheckpointManager::CheckpointManager(
    std::shared_ptr<Store> store,
    std::shared_ptr<WALManager> wal,
    std::shared_ptr<Server> server,
    const std::string &data_file_path,
    const std::string &wal_file_path)
    : store_(store),
      wal_(wal),
      server_(server),
      data_file_path_(data_file_path),
      wal_file_path_(wal_file_path)
{
}

bool CheckpointManager::createCheckpoint()
{
    if (!store_ || !wal_ || !server_)
    {
        return false;
    }

    /*
        WAL rule:
        Ensure all committed WAL durable first.
    */
    wal_->fsync();

    uint64_t checkpoint_lsn = wal_->getFlushedLSN();

    std::string temp_file = data_file_path_ + ".tmp";

    /*
        1. Write snapshot to temp file
    */
    writeSnapshotFile(temp_file, checkpoint_lsn);

    /*
        2. fsync temp file
    */
    int fd = ::open(temp_file.c_str(), O_RDWR);

    if (fd >= 0)
    {
        ::fsync(fd);
        ::close(fd);
    }

    /*
        3. Atomic rename
    */
    std::filesystem::rename(temp_file, data_file_path_);

    /*
        4. Truncate WAL up to checkpoint_lsn
    */
    truncateWAL(checkpoint_lsn);

    return true;
}

void CheckpointManager::writeSnapshotFile(
    const std::string &temp_path,
    uint64_t checkpoint_lsn)
{
    std::ofstream out(temp_path, std::ios::binary | std::ios::trunc);

    if (!out.is_open())
        return;

    auto entries = store_->getAllEntries();

    uint32_t magic = 0x4B564442;
    uint32_t version = 1;

    uint64_t count = entries.size();

    // HEADER
    out.write(reinterpret_cast<char *>(&magic), sizeof(magic));
    out.write(reinterpret_cast<char *>(&version), sizeof(version));
    out.write(reinterpret_cast<char *>(&checkpoint_lsn), sizeof(checkpoint_lsn));
    out.write(reinterpret_cast<char *>(&count), sizeof(count));

    // ENTRIES
    for (const auto &pair : entries)
    {
        const Key &key = pair.first;
        const Value &value = pair.second;

        uint16_t key_len = static_cast<uint16_t>(key.size());
        uint16_t value_len = static_cast<uint16_t>(value.size());

        out.write(reinterpret_cast<char *>(&key_len), sizeof(key_len));
        out.write(key.data(), key_len);

        out.write(reinterpret_cast<char *>(&value_len), sizeof(value_len));
        out.write(value.data(), value_len);
    }

    out.flush();

    int fd = ::open(temp_path.c_str(), O_RDWR);
    if (fd >= 0)
    {
        ::fsync(fd);
        ::close(fd);
    }
}

void CheckpointManager::truncateWAL(uint64_t checkpoint_lsn)
{
    std::ifstream in(wal_file_path_, std::ios::binary);

    if (!in.is_open())
        return;

    std::string remaining;

    while (true)
    {
        std::streampos record_start = in.tellg();

        uint32_t crc;
        in.read(reinterpret_cast<char *>(&crc), sizeof(crc));
        if (!in)
            break;

        uint64_t lsn;
        uint32_t txn_id;
        uint8_t type;

        in.read(reinterpret_cast<char *>(&lsn), sizeof(lsn));
        in.read(reinterpret_cast<char *>(&txn_id), sizeof(txn_id));
        in.read(reinterpret_cast<char *>(&type), sizeof(type));

        if (!in)
            break;

        uint16_t key_len;
        in.read(reinterpret_cast<char *>(&key_len), sizeof(key_len));
        if (!in)
            break;

        std::string key(key_len, '\0');
        in.read(&key[0], key_len);
        if (!in)
            break;

        uint16_t value_len = 0;
        if (type == 2 /*PUT*/ || type == 3 /*DELETE*/)
        {
            in.read(reinterpret_cast<char *>(&value_len), sizeof(value_len));
            if (!in)
                break;
        }

        std::string value(value_len, '\0');
        if (value_len > 0)
        {
            in.read(&value[0], value_len);
            if (!in)
                break;
        }

        /*
            Reconstruct EXACT bytes by re-reading original segment
            instead of manual rebuild
        */
        std::streampos record_end = in.tellg();
        std::streamsize size = record_end - record_start;

        in.seekg(record_start);

        std::string raw(size, '\0');
        in.read(raw.data(), size);

        if (!in)
            break;

        if (lsn > checkpoint_lsn)
        {
            remaining.append(raw);
        }
    }

    in.close();

    std::ofstream out(wal_file_path_, std::ios::binary | std::ios::trunc);
    if (!out.is_open())
        return;

    out.write(remaining.data(), remaining.size());
    out.flush();

    int fd = ::open(wal_file_path_.c_str(), O_RDWR);
    if (fd >= 0)
    {
        ::fsync(fd);
        ::close(fd);
    }
}
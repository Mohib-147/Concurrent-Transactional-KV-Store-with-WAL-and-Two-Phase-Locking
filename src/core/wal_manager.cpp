#include "wal_manager.h"
#include <cstring>
#include <cstdint>
#include <iostream>
#include <fcntl.h>
#include <unistd.h>

WALManager::WALManager(const std::string &file_path)
    : file_path_(file_path)
{
    log_file_.open(file_path_, std::ios::binary | std::ios::app);
}

WALManager::~WALManager()
{
    if (log_file_.is_open())
        log_file_.close();
}

uint64_t WALManager::logBegin(TxnId txn_id)
{
    std::lock_guard<std::mutex> lock(mtx_);

    uint64_t lsn = next_lsn_++;

    buffer_.push_back(
        serializeRecord(lsn, txn_id, RecordType::BEGIN, "", ""));

    return lsn;
}

uint64_t WALManager::logPut(TxnId txn_id, const Key &key, const Value &value)
{
    std::lock_guard<std::mutex> lock(mtx_);

    uint64_t lsn = next_lsn_++;

    buffer_.push_back(
        serializeRecord(lsn, txn_id, RecordType::PUT, key, value));

    return lsn;
}

uint64_t WALManager::logDelete(TxnId txn_id, const Key &key)
{
    std::lock_guard<std::mutex> lock(mtx_);

    uint64_t lsn = next_lsn_++;

    buffer_.push_back(
        serializeRecord(lsn, txn_id, RecordType::DELETE, key, ""));

    return lsn;
}

uint64_t WALManager::logCommit(TxnId txn_id)
{
    std::lock_guard<std::mutex> lock(mtx_);

    uint64_t lsn = next_lsn_++;

    buffer_.push_back(
        serializeRecord(lsn, txn_id, RecordType::COMMIT, "", ""));

    return lsn;
}

uint64_t WALManager::logAbort(TxnId txn_id)
{
    std::lock_guard<std::mutex> lock(mtx_);

    uint64_t lsn = next_lsn_++;

    buffer_.push_back(
        serializeRecord(lsn, txn_id, RecordType::ABORT, "", ""));

    return lsn;
}

std::vector<std::string> WALManager::fetchAndClearBuffer()
{
    std::lock_guard<std::mutex> lock(mtx_);

    std::vector<std::string> out;
    out.swap(buffer_);

    return out;
}

void WALManager::writeToDisk(const std::vector<std::string> &records)
{
    if (!log_file_.is_open())
        return;

    for (const auto &rec : records)
    {
        log_file_.write(rec.data(), rec.size());
    }

    log_file_.flush();
}

std::string WALManager::serializeRecord(uint64_t lsn,
                                        TxnId txn_id,
                                        RecordType type,
                                        const Key &key,
                                        const Value &value)
{
    std::string body;
    body.reserve(64 + key.size() + value.size());

    auto append_u64 = [&](uint64_t v)
    {
        for (int i = 0; i < 8; i++)
            body.push_back(static_cast<char>((v >> (i * 8)) & 0xFF));
    };

    auto append_u32 = [&](uint32_t v)
    {
        for (int i = 0; i < 4; i++)
            body.push_back(static_cast<char>((v >> (i * 8)) & 0xFF));
    };

    auto append_str = [&](const std::string &s)
    {
        uint16_t len = static_cast<uint16_t>(s.size());
        body.push_back(static_cast<char>(len & 0xFF));
        body.push_back(static_cast<char>((len >> 8) & 0xFF));
        body.append(s);
    };

    // HEADER
    append_u64(lsn);
    append_u32(static_cast<uint32_t>(txn_id));
    body.push_back(static_cast<char>(type));

    // PAYLOAD
    append_str(key);
    append_str(value);

    uint32_t crc = crc32(body);

    std::string final;
    final.reserve(body.size() + 4);

    final.push_back(static_cast<char>(crc & 0xFF));
    final.push_back(static_cast<char>((crc >> 8) & 0xFF));
    final.push_back(static_cast<char>((crc >> 16) & 0xFF));
    final.push_back(static_cast<char>((crc >> 24) & 0xFF));

    final += body;

    return final;
}

uint32_t WALManager::crc32(const std::string &data)
{
    uint32_t crc = 0xFFFFFFFF;

    for (unsigned char c : data)
    {
        crc ^= c;

        for (int i = 0; i < 8; i++)
        {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xEDB88320;
            else
                crc >>= 1;
        }
    }

    return ~crc;
}

uint64_t WALManager::getFlushedLSN() const
{
    std::lock_guard<std::mutex> lock(mtx_);
    return flushed_lsn_;
}

void WALManager::updateFlushedLSN(uint64_t lsn)
{
    std::lock_guard<std::mutex> lock(mtx_);
    if (lsn > flushed_lsn_)
        flushed_lsn_ = lsn;
}

void WALManager::fsync()
{
    int fd = ::open(file_path_.c_str(), O_RDWR);
    if (fd >= 0)
    {
        ::fsync(fd);
        ::close(fd);
    }
}
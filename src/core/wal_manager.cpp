#include "wal_manager.h"
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <iostream>

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

// ------------------------- helpers -------------------------

static void append_u64(std::string &b, uint64_t v)
{
    for (int i = 0; i < 8; i++)
        b.push_back(static_cast<char>((v >> (8 * i)) & 0xFF));
}

static void append_u32(std::string &b, uint32_t v)
{
    for (int i = 0; i < 4; i++)
        b.push_back(static_cast<char>((v >> (8 * i)) & 0xFF));
}

static void append_str(std::string &b, const std::string &s)
{
    uint16_t len = static_cast<uint16_t>(s.size());
    b.push_back(static_cast<char>(len & 0xFF));
    b.push_back(static_cast<char>((len >> 8) & 0xFF));
    b.append(s);
}

// ------------------------- API -------------------------

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

// ------------------------- buffer -------------------------

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

        if (rec.size() >= 12)
        {
            uint64_t lsn;
            std::memcpy(&lsn, rec.data() + 4, sizeof(uint64_t));
            updateFlushedLSN(lsn);
        }
    }

    log_file_.flush();
}

// ------------------------- serialization -------------------------

std::string WALManager::serializeRecord(uint64_t lsn,
                                        TxnId txn_id,
                                        RecordType type,
                                        const Key &key,
                                        const Value &value)
{
    std::string body;

    // header (WITHOUT CRC)
    append_u64(body, lsn);
    append_u32(body, txn_id);
    body.push_back(static_cast<char>(type));

    // payload rules (STRICT MANUAL)

    if (type == RecordType::PUT)
    {
        append_str(body, key);
        append_str(body, value);
    }
    else if (type == RecordType::DELETE)
    {
        append_str(body, key);
    }
    else
    {
        // BEGIN / COMMIT / ABORT → NO PAYLOAD
    }

    // CRC over everything after crc field
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

// ------------------------- CRC -------------------------

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

// ------------------------- fsync -------------------------

void WALManager::fsync()
{
    int fd = ::open(file_path_.c_str(), O_RDWR);
    if (fd >= 0)
    {
        ::fsync(fd);
        ::close(fd);
    }
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
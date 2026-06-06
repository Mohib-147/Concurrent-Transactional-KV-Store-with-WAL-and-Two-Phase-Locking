#ifndef WAL_MANAGER_H
#define WAL_MANAGER_H

#include "kvdb.h"
#include <vector>
#include <string>
#include <mutex>
#include <cstdint>
#include <fstream>

class WALManager
{
public:
    enum class RecordType : uint8_t
    {
        BEGIN = 1,
        PUT = 2,
        DELETE = 3,
        COMMIT = 4,
        ABORT = 5
    };

    struct WALRecord
    {
        uint64_t lsn;
        TxnId txn_id;
        RecordType type;

        Key key;
        Value value;

        std::string raw;
    };

    explicit WALManager(const std::string &file_path);
    ~WALManager();

    uint64_t logBegin(TxnId txn_id);
    uint64_t logPut(TxnId txn_id, const Key &key, const Value &value);
    uint64_t logDelete(TxnId txn_id, const Key &key);
    uint64_t logCommit(TxnId txn_id);
    uint64_t logAbort(TxnId txn_id);

    std::vector<std::string> fetchAndClearBuffer();

    void writeToDisk(const std::vector<std::string> &records);

    uint64_t getFlushedLSN() const;
    void updateFlushedLSN(uint64_t lsn);

    void fsync();

private:
    std::string file_path_;
    std::ofstream log_file_;

    mutable std::mutex mtx_;

    uint64_t next_lsn_ = 1;
    uint64_t flushed_lsn_ = 0;

    std::vector<std::string> buffer_;

    std::string serializeRecord(uint64_t lsn,
                                TxnId txn_id,
                                RecordType type,
                                const Key &key,
                                const Value &value);

    uint32_t crc32(const std::string &data);
};

#endif
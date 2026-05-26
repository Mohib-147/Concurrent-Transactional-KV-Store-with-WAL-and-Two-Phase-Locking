#ifndef RECOVERY_MANAGER_H
#define RECOVERY_MANAGER_H

#include "kvdb.h"
#include "store.h"
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <memory>

class RecoveryManager
{
public:
    explicit RecoveryManager(std::shared_ptr<Store> store,
                             const std::string &wal_file);

    void recover();

private:
    enum class RecordType : uint8_t
    {
        BEGIN = 1,
        PUT = 2,
        DELETE = 3,
        COMMIT = 4,
        ABORT = 5
    };

    struct Operation
    {
        enum class Type
        {
            PUT,
            DELETE
        } type;
        Key key;
        Value value;
    };

    struct TxnState
    {
        std::vector<Operation> ops;
        bool committed = false;
        uint64_t commit_lsn = 0;
    };

    std::shared_ptr<Store> store_;
    std::string wal_file_;

    uint32_t crc32(const std::string &data);
    bool verifyCRC(const std::string &record);

    uint64_t readU64(const char *data);
    uint32_t readU32(const char *data);
};

#endif
#ifndef RECOVERY_MANAGER_H
#define RECOVERY_MANAGER_H

#include "kvdb.h"
#include "store.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <cstdint>

class RecoveryManager
{
public:
    RecoveryManager(std::shared_ptr<Store> store,
                    const std::string &data_file,
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

private:
    std::shared_ptr<Store> store_;

    std::string data_file_;
    std::string wal_file_;

    uint64_t checkpoint_lsn_ = 0;

private:
    void loadSnapshot();

    uint32_t crc32(const std::string &data);

    uint64_t readU64(const char *p);
    uint32_t readU32(const char *p);
};

#endif
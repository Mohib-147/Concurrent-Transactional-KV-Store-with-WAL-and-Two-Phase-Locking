#ifndef KVDB_H
#define KVDB_H

#include <string>
#include <cstdint>
#include <map>
#include <set>
#include <vector>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <deque>

using TxnId = uint32_t;
using Key = std::string;
using Value = std::string;
using SessionId = uint32_t;

enum class LockMode
{
    NONE = 0,
    SHARED = 1,
    EXCLUSIVE = 2
};

inline std::string lockModeToString(LockMode mode)
{
    switch (mode)
    {
    case LockMode::NONE:
        return "NONE";
    case LockMode::SHARED:
        return "SHARED";
    case LockMode::EXCLUSIVE:
        return "EXCLUSIVE";
    default:
        return "UNKNOWN";
    }
}

enum class IsolationLevel
{
    READ_COMMITTED,
    REPEATABLE_READ
};

inline IsolationLevel parseIsolationLevel(const std::string &str)
{
    if (str == "READ_COMMITTED")
    {
        return IsolationLevel::READ_COMMITTED;
    }
    return IsolationLevel::REPEATABLE_READ;
}

enum class TxnState
{
    IDLE,
    ACTIVE,
    COMMITTED,
    ABORTED
};

struct LockWaiter
{
    TxnId txn_id;
    LockMode requested_mode;
    std::shared_ptr<std::condition_variable> cv;
    std::shared_ptr<bool> granted;
    std::shared_ptr<bool> deadlocked;
};

struct LockEntry
{
    LockMode current_mode = LockMode::NONE;
    std::set<TxnId> holders;
    std::deque<LockWaiter> wait_queue;
};

struct TransactionInfo
{
    TxnId txn_id;
    SessionId session_id;
    IsolationLevel isolation_level;
    TxnState state = TxnState::IDLE;

    std::map<Key, std::pair<Value, bool>> pending_buffer;
    std::map<Key, LockMode> locks_held;

    std::chrono::system_clock::time_point txn_start_time;
};

struct Response
{
    bool success;
    std::string message;
    std::string data;
};

#endif
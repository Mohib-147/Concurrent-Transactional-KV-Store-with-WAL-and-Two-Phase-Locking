#include "lock_manager.h"
#include <iostream>
#include <sstream>
#include <chrono>
#include <algorithm>

LockManager::LockManager() {}

LockManager::~LockManager() {}

bool LockManager::isCompatible(LockMode mode1, LockMode mode2)
{
    if (mode1 == LockMode::NONE || mode2 == LockMode::NONE)
        return true;
    if (mode1 == LockMode::SHARED && mode2 == LockMode::SHARED)
        return true;
    return false;
}

bool LockManager::acquireLock(TxnId txn_id, const Key &key, LockMode mode, int timeout_ms)
{
    std::unique_lock<std::mutex> lock(lock_table_mutex_);

    if (lock_table_.find(key) == lock_table_.end())
    {
        lock_table_[key] = LockEntry();
    }

    LockEntry &entry = lock_table_[key];

    auto it = entry.holders.find(txn_id);
    if (it != entry.holders.end())
    {
        if (entry.current_mode == LockMode::EXCLUSIVE || mode == LockMode::SHARED)
            return true;

        if (entry.holders.size() == 1)
        {
            entry.current_mode = LockMode::EXCLUSIVE;
            return true;
        }
        goto add_to_wait_queue;
    }

    if (isCompatible(entry.current_mode, mode) && entry.wait_queue.empty())
    {
        entry.holders.insert(txn_id);
        if (entry.current_mode == LockMode::NONE)
            entry.current_mode = mode;
        else if (mode == LockMode::EXCLUSIVE)
            entry.current_mode = LockMode::EXCLUSIVE;
        return true;
    }

add_to_wait_queue:
{
    auto granted_flag = std::make_shared<bool>(false);
    auto deadlocked_flag = std::make_shared<bool>(false);

    LockWaiter waiter;
    waiter.txn_id = txn_id;
    waiter.requested_mode = mode;
    waiter.granted = granted_flag;
    waiter.deadlocked = deadlocked_flag;
    waiter.cv = std::make_shared<std::condition_variable>();

    entry.wait_queue.push_back(waiter);
    auto cv = entry.wait_queue.back().cv; // capture before we release the lock

    auto timeout = std::chrono::milliseconds(timeout_ms);
    bool notified = cv->wait_for(lock, timeout, [&granted_flag, &deadlocked_flag]()
                                 { return *granted_flag || *deadlocked_flag; });

    if (*deadlocked_flag)
    {
        auto &q = entry.wait_queue;
        q.erase(std::remove_if(q.begin(), q.end(),
                               [txn_id](const LockWaiter &w)
                               { return w.txn_id == txn_id; }),
                q.end());
        return false;
    }

    if (!notified)
    {
        auto &q = entry.wait_queue;
        q.erase(std::remove_if(q.begin(), q.end(),
                               [txn_id](const LockWaiter &w)
                               { return w.txn_id == txn_id; }),
                q.end());
        return false;
    }

    return true;
}
}

void LockManager::releaseAllLocks(TxnId txn_id)
{
    std::unique_lock<std::mutex> lock(lock_table_mutex_);

    std::vector<Key> keys_to_process;
    for (const auto &pair : lock_table_)
    {
        if (pair.second.holders.find(txn_id) != pair.second.holders.end())
            keys_to_process.push_back(pair.first);
    }

    for (const Key &key : keys_to_process)
    {
        LockEntry &entry = lock_table_[key];
        entry.holders.erase(txn_id);

        if (entry.holders.empty())
        {
            entry.current_mode = LockMode::NONE;
            processWaiters(key);
        }
    }
}

void LockManager::releaseLock(TxnId txn_id, const Key &key)
{
    std::unique_lock<std::mutex> lock(lock_table_mutex_);

    auto it = lock_table_.find(key);
    if (it == lock_table_.end())
        return;

    LockEntry &entry = it->second;
    entry.holders.erase(txn_id);

    if (entry.holders.empty())
    {
        entry.current_mode = LockMode::NONE;
        processWaiters(key);
    }
}

LockMode LockManager::hasLock(TxnId txn_id, const Key &key) const
{
    std::unique_lock<std::mutex> lock(lock_table_mutex_);

    auto it = lock_table_.find(key);
    if (it == lock_table_.end())
        return LockMode::NONE;

    const LockEntry &entry = it->second;
    if (entry.holders.find(txn_id) != entry.holders.end())
        return entry.current_mode;

    return LockMode::NONE;
}

void LockManager::processWaiters(const Key &key)
{
    auto it = lock_table_.find(key);
    if (it == lock_table_.end())
        return;

    LockEntry &entry = it->second;

    while (!entry.wait_queue.empty())
    {
        LockWaiter &waiter = entry.wait_queue.front();

        if (!isCompatible(entry.current_mode, waiter.requested_mode))
            break;

        entry.holders.insert(waiter.txn_id);
        if (entry.current_mode == LockMode::NONE)
            entry.current_mode = waiter.requested_mode;
        else if (waiter.requested_mode == LockMode::EXCLUSIVE)
            entry.current_mode = LockMode::EXCLUSIVE;

        *waiter.granted = true;
        auto cv = waiter.cv;

        entry.wait_queue.erase(entry.wait_queue.begin());
        cv->notify_one();
    }
}

std::string LockManager::dumpLockTable() const
{
    std::unique_lock<std::mutex> lock(lock_table_mutex_);

    std::stringstream ss;
    ss << "=== Lock Table ===\n";

    if (lock_table_.empty())
    {
        ss << "No locks held.\n";
        return ss.str();
    }

    for (const auto &pair : lock_table_)
    {
        const Key &key = pair.first;
        const LockEntry &entry = pair.second;

        ss << "Key: \"" << key << "\"\n";
        ss << "  Mode: " << lockModeToString(entry.current_mode) << "\n";
        ss << "  Holders: ";
        for (TxnId holder : entry.holders)
            ss << holder << " ";
        ss << "\n";

        if (!entry.wait_queue.empty())
        {
            ss << "  Waiters: ";
            for (const auto &w : entry.wait_queue)
                ss << "Txn" << w.txn_id << "(" << lockModeToString(w.requested_mode) << ") ";
            ss << "\n";
        }
    }

    return ss.str();
}

size_t LockManager::getLockedKeyCount() const
{
    std::unique_lock<std::mutex> lock(lock_table_mutex_);
    return lock_table_.size();
}

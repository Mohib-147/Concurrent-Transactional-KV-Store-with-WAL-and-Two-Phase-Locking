#include "lock_manager.h"
#include <iostream>
#include <sstream>
#include <chrono>

LockManager::LockManager() {}

LockManager::~LockManager() {}

bool LockManager::isCompatible(LockMode mode1, LockMode mode2)
{

    if (mode1 == LockMode::NONE || mode2 == LockMode::NONE)
    {
        return true;
    }

    if (mode1 == LockMode::SHARED && mode2 == LockMode::SHARED)
    {
        return true;
    }

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

        if (isCompatible(entry.current_mode, mode))
        {
            if (entry.current_mode == LockMode::NONE)
            {
                entry.current_mode = mode;
            }
            else if (mode == LockMode::EXCLUSIVE)
            {
                if (entry.holders.size() == 1)
                {
                    entry.current_mode = LockMode::EXCLUSIVE;
                }
                else
                {
                    goto add_to_wait_queue;
                }
            }
            return true;
        }
    }

    if (isCompatible(entry.current_mode, mode) && entry.wait_queue.empty())
    {
        entry.holders.insert(txn_id);
        if (entry.current_mode == LockMode::NONE)
        {
            entry.current_mode = mode;
        }
        else if (mode == LockMode::EXCLUSIVE && entry.holders.size() == 1)
        {
            entry.current_mode = LockMode::EXCLUSIVE;
        }
        return true;
    }

add_to_wait_queue:
{
    LockWaiter waiter;
    waiter.txn_id = txn_id;
    waiter.requested_mode = mode;
    waiter.granted = false;
    waiter.deadlocked = false;

    entry.wait_queue.push_back(waiter);
    auto waiter_index = entry.wait_queue.size() - 1;

    auto timeout = std::chrono::milliseconds(timeout_ms);
    auto result = entry.wait_queue[waiter_index].cv.wait_for(lock, timeout, [&]()
                                                             { return entry.wait_queue[waiter_index].granted ||
                                                                      entry.wait_queue[waiter_index].deadlocked; });

    if (entry.wait_queue[waiter_index].deadlocked)
    {
        if (waiter_index < entry.wait_queue.size() &&
            entry.wait_queue[waiter_index].txn_id == txn_id)
        {
            entry.wait_queue.erase(entry.wait_queue.begin() + waiter_index);
        }
        return false;
    }

    if (!result)
    {
        if (waiter_index < entry.wait_queue.size() &&
            entry.wait_queue[waiter_index].txn_id == txn_id)
        {
            entry.wait_queue.erase(entry.wait_queue.begin() + waiter_index);
        }
        return false;
    }

    if (entry.wait_queue[waiter_index].granted)
    {
        if (waiter_index < entry.wait_queue.size() &&
            entry.wait_queue[waiter_index].txn_id == txn_id)
        {
            entry.wait_queue.erase(entry.wait_queue.begin() + waiter_index);
        }
        return true;
    }
}

    return false;
}

void LockManager::releaseAllLocks(TxnId txn_id)
{
    std::unique_lock<std::mutex> lock(lock_table_mutex_);

    // Collect keys to process (can't modify map while iterating)
    std::vector<Key> keys_to_process;
    for (const auto &pair : lock_table_)
    {
        const Key &key = pair.first;
        const LockEntry &entry = pair.second;

        // Check if this txn holds a lock on this key
        if (entry.holders.find(txn_id) != entry.holders.end())
        {
            keys_to_process.push_back(key);
        }
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
    {
        return;
    }

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
    {
        return LockMode::NONE;
    }

    const LockEntry &entry = it->second;

    if (entry.holders.find(txn_id) != entry.holders.end())
    {
        return entry.current_mode;
    }

    return LockMode::NONE;
}

void LockManager::processWaiters(const Key &key)
{
    auto it = lock_table_.find(key);
    if (it == lock_table_.end())
    {
        return;
    }

    LockEntry &entry = it->second;

    while (!entry.wait_queue.empty())
    {
        LockWaiter &waiter = entry.wait_queue.front();

        if (isCompatible(entry.current_mode, waiter.requested_mode))
        {
            entry.holders.insert(waiter.txn_id);

            if (entry.current_mode == LockMode::NONE)
            {
                entry.current_mode = waiter.requested_mode;
            }
            else if (waiter.requested_mode == LockMode::EXCLUSIVE)
            {
                entry.current_mode = LockMode::EXCLUSIVE;
            }

            waiter.granted = true;
            waiter.cv.notify_one();

            entry.wait_queue.erase(entry.wait_queue.begin());
        }
        else
        {
            break;
        }
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
        {
            ss << holder << " ";
        }
        ss << "\n";

        if (!entry.wait_queue.empty())
        {
            ss << "  Waiters: ";
            for (const auto &waiter : entry.wait_queue)
            {
                ss << "Txn" << waiter.txn_id << "(" << lockModeToString(waiter.requested_mode) << ") ";
            }
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
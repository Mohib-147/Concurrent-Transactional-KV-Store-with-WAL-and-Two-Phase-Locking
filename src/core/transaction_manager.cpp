#include "transaction_manager.h"
#include "lock_manager.h"
#include "store.h"
#include "server.h"
#include "wal_manager.h"
#include "group_commit.h"
#include <iostream>
#include <sstream>

TransactionManager::TransactionManager(SessionId session_id, std::shared_ptr<Server> server)
    : session_id_(session_id), server_(server)
{
    current_txn_.session_id = session_id;
    current_txn_.state = TxnState::IDLE;
    current_txn_.txn_id = 0;
}

TransactionManager::~TransactionManager() {}

std::string TransactionManager::begin(IsolationLevel isolation_level)
{
    if (current_txn_.state == TxnState::ACTIVE)
    {
        return "Already in transaction (txn " + std::to_string(current_txn_.txn_id) + ")";
    }

    if (!server_)
    {
        return "Server not initialized";
    }

    TxnId new_id = server_->allocateTxnId();
    current_txn_.txn_id = new_id;
    current_txn_.session_id = session_id_;
    current_txn_.isolation_level = isolation_level;
    current_txn_.state = TxnState::ACTIVE;
    current_txn_.pending_buffer.clear();
    current_txn_.locks_held.clear();
    current_txn_.txn_start_time = std::chrono::system_clock::now();

    if (wal_)
        wal_->logBegin(new_id);

    return "";
}

std::string TransactionManager::commit()
{
    if (current_txn_.state != TxnState::ACTIVE)
    {
        return "Not in transaction";
    }

    TxnId txn_id = current_txn_.txn_id;

    if (wal_)
    {
        for (const auto &pair : current_txn_.pending_buffer)
        {
            const Key &key = pair.first;
            const Value &value = pair.second.first;
            bool is_deleted = pair.second.second;
            if (is_deleted)
                wal_->logDelete(txn_id, key);
            else
                wal_->logPut(txn_id, key, value);
        }
    }

    uint64_t commit_lsn = 0;
    if (wal_)
    {
        commit_lsn = wal_->logCommit(txn_id);
    }

    if (group_commit_)
    {
        group_commit_->waitForCommit(commit_lsn);
    }

    applyPendingBuffer();

    if (lock_manager_)
    {
        lock_manager_->releaseAllLocks(txn_id);
    }

    total_commits_++;
    current_txn_ = TransactionInfo{};
    current_txn_.session_id = session_id_;
    current_txn_.state = TxnState::IDLE;

    return "";
}

std::string TransactionManager::abort()
{
    if (current_txn_.state != TxnState::ACTIVE)
    {
        return "Not in transaction";
    }

    TxnId txn_id = current_txn_.txn_id;

    if (wal_)
        wal_->logAbort(txn_id);

    current_txn_.pending_buffer.clear();

    if (lock_manager_)
    {
        lock_manager_->releaseAllLocks(txn_id);
    }

    current_txn_ = TransactionInfo{};
    current_txn_.session_id = session_id_;
    current_txn_.state = TxnState::IDLE;

    return "";
}

std::string TransactionManager::put(const Key &key, const Value &value)
{
    if (current_txn_.state != TxnState::ACTIVE)
    {
        return "Not in transaction";
    }
    if (!lock_manager_)
    {
        return "Lock manager not initialized";
    }

    bool lock_acquired = lock_manager_->acquireLock(current_txn_.txn_id, key, LockMode::EXCLUSIVE, 3000);
    if (!lock_acquired)
    {
        abort();
        deadlock_aborts_++;
        return "Deadlock detected, transaction aborted";
    }

    current_txn_.locks_held[key] = LockMode::EXCLUSIVE;
    current_txn_.pending_buffer[key] = {value, false};

    return "";
}

std::string TransactionManager::delete_key(const Key &key)
{
    if (current_txn_.state != TxnState::ACTIVE)
    {
        return "Not in transaction";
    }
    if (!lock_manager_)
    {
        return "Lock manager not initialized";
    }

    bool lock_acquired = lock_manager_->acquireLock(current_txn_.txn_id, key, LockMode::EXCLUSIVE, 3000);
    if (!lock_acquired)
    {
        abort();
        deadlock_aborts_++;
        return "Deadlock detected, transaction aborted";
    }

    current_txn_.locks_held[key] = LockMode::EXCLUSIVE;
    current_txn_.pending_buffer[key] = {"", true};

    return "";
}

std::string TransactionManager::get(const Key &key, Value &out_value)
{
    if (current_txn_.state != TxnState::ACTIVE)
        return "Not in transaction";
    if (!lock_manager_)
        return "Lock manager not initialized";

    LockMode lock_mode = LockMode::SHARED;

    bool lock_acquired = lock_manager_->acquireLock(
        current_txn_.txn_id,
        key,
        lock_mode,
        3000);

    if (!lock_acquired)
    {
        abort();
        deadlock_aborts_++;
        return "Deadlock detected, transaction aborted";
    }

    current_txn_.locks_held[key] = lock_mode;

    auto it = current_txn_.pending_buffer.find(key);
    if (it != current_txn_.pending_buffer.end())
    {
        if (it->second.second)
        {
            if (current_txn_.isolation_level == IsolationLevel::READ_COMMITTED)
            {
                lock_manager_->releaseLock(current_txn_.txn_id, key);
                current_txn_.locks_held.erase(key);
            }
            return "Key not found (deleted in this txn)";
        }
        else
        {
            out_value = it->second.first;
            if (current_txn_.isolation_level == IsolationLevel::READ_COMMITTED)
            {
                lock_manager_->releaseLock(current_txn_.txn_id, key);
                current_txn_.locks_held.erase(key);
            }
            return "";
        }
    }

    if (!store_)
        return "Store not initialized";

    if (store_->get(key, out_value))
    {
        if (current_txn_.isolation_level == IsolationLevel::READ_COMMITTED)
        {
            lock_manager_->releaseLock(current_txn_.txn_id, key);
            current_txn_.locks_held.erase(key);
        }
        return "";
    }
    else
    {
        if (current_txn_.isolation_level == IsolationLevel::READ_COMMITTED)
        {
            lock_manager_->releaseLock(current_txn_.txn_id, key);
            current_txn_.locks_held.erase(key);
        }
        return "Key not found";
    }
}

bool TransactionManager::isInTransaction() const
{
    return current_txn_.state == TxnState::ACTIVE;
}

TxnId TransactionManager::getCurrentTxnId() const
{
    return current_txn_.txn_id;
}

IsolationLevel TransactionManager::getIsolationLevel() const
{
    return current_txn_.isolation_level;
}

std::string TransactionManager::getStats() const
{
    std::stringstream ss;
    ss << "session: " << session_id_ << "\n";

    if (current_txn_.state == TxnState::ACTIVE)
    {
        ss << "active txn: " << current_txn_.txn_id << "\n";
        ss << "locks held: " << current_txn_.locks_held.size() << "\n";
    }
    else
    {
        ss << "active txn: none\n";
        ss << "locks held: 0\n";
    }

    ss << "total commits: " << total_commits_ << "\n";
    ss << "deadlock aborts: " << deadlock_aborts_;
    return ss.str();
}

std::vector<Key> TransactionManager::getHeldLocks() const
{
    std::vector<Key> keys;
    for (const auto &pair : current_txn_.locks_held)
    {
        keys.push_back(pair.first);
    }
    return keys;
}

void TransactionManager::setGlobalReferences(std::shared_ptr<Store> store,
                                             std::shared_ptr<LockManager> lock_manager,
                                             std::shared_ptr<WALManager> wal,
                                             std::shared_ptr<GroupCommitManager> group_commit)
{
    store_ = store;
    lock_manager_ = lock_manager;
    wal_ = wal;
    group_commit_ = group_commit;
}

void TransactionManager::applyPendingBuffer()
{
    if (!store_)
        return;

    for (const auto &pair : current_txn_.pending_buffer)
    {
        const Key &key = pair.first;
        const Value &value = pair.second.first;
        bool is_deleted = pair.second.second;

        if (is_deleted)
            store_->delete_key(key);
        else
            store_->put(key, value);
    }

    current_txn_.pending_buffer.clear();
}
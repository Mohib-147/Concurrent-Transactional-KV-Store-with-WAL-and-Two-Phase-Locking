#ifndef TRANSACTION_MANAGER_H
#define TRANSACTION_MANAGER_H

#include "kvdb.h"
#include "lock_manager.h"
#include <memory>

class Store;
class Server;
class WALManager;
class GroupCommitManager;

/**
 * TransactionManager - Manages per-client transaction state.
 *
 * One instance per connected client. Tracks:
 * - Active transaction (if any)
 * - Pending modifications
 * - Isolation level
 * - Locks held
 * - Statistics
 */
class TransactionManager
{
public:
    TransactionManager(SessionId session_id, std::shared_ptr<Server> server);
    ~TransactionManager();

    /**
     * Begin a new transaction.
     * @return error string if already in transaction, empty string if success
     */
    std::string begin(IsolationLevel isolation_level = IsolationLevel::REPEATABLE_READ);

    /**
     * Commit current transaction (apply pending buffer to store).
     * Must already be durably logged (Phase 2+).
     */
    std::string commit();

    /**
     * Abort current transaction (discard pending buffer).
     */
    std::string abort();

    /**
     * Queue a PUT operation in pending buffer.
     */
    std::string put(const Key &key, const Value &value);

    std::string delete_key(const Key &key);

    std::string get(const Key &key, Value &out_value);

    bool isInTransaction() const;

    TxnId getCurrentTxnId() const;

    IsolationLevel getIsolationLevel() const;

    std::string getStats() const;

    std::vector<Key> getHeldLocks() const;

    void setGlobalReferences(std::shared_ptr<Store> store,
                             std::shared_ptr<LockManager> lock_manager,
                             std::shared_ptr<WALManager> wal,
                             std::shared_ptr<GroupCommitManager> group_commit);

private:
    SessionId session_id_;
    TransactionInfo current_txn_;
    TxnId next_txn_id_ = 1;

    std::shared_ptr<Server> server_;

    std::shared_ptr<Store> store_;
    std::shared_ptr<LockManager> lock_manager_;
    std::shared_ptr<WALManager> wal_;
    std::shared_ptr<GroupCommitManager> group_commit_;

    uint64_t total_commits_ = 0;
    uint64_t deadlock_aborts_ = 0;

    void applyPendingBuffer();
};

#endif
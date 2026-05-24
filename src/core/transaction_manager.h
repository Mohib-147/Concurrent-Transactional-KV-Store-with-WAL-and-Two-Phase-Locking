#ifndef TRANSACTION_MANAGER_H
#define TRANSACTION_MANAGER_H

#include "kvdb.h"
#include "lock_manager.h"
#include <memory>

class Store;

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
    explicit TransactionManager(SessionId session_id);
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

    /**
     * Queue a DELETE operation in pending buffer.
     */
    std::string delete_key(const Key &key);

    /**
     * Queue a GET operation and return the value.
     * This acquires locks but doesn't modify the buffer.
     */
    std::string get(const Key &key, Value &out_value);

    /**
     * Check if currently in a transaction.
     */
    bool isInTransaction() const;

    /**
     * Get current transaction ID.
     */
    TxnId getCurrentTxnId() const;

    /**
     * Get current isolation level.
     */
    IsolationLevel getIsolationLevel() const;

    /**
     * Get session statistics.
     */
    std::string getStats() const;

    /**
     * Get locks held by current transaction.
     */
    std::vector<Key> getHeldLocks() const;

    /**
     * Set references to global Store and LockManager (called by Server).
     */
    void setGlobalReferences(std::shared_ptr<Store> store,
                             std::shared_ptr<LockManager> lock_manager);

private:
    SessionId session_id_;
    TransactionInfo current_txn_;
    TxnId next_txn_id_ = 1;

    // Global references (injected by Server)
    std::shared_ptr<Store> store_;
    std::shared_ptr<LockManager> lock_manager_;

    // Statistics
    uint64_t total_commits_ = 0;
    uint64_t deadlock_aborts_ = 0;

    /**
     * Helper: apply pending buffer to store (called from commit).
     * Must be called after WAL fsync is guaranteed (Phase 2+).
     */
    void applyPendingBuffer();
};

#endif // TRANSACTION_MANAGER_H
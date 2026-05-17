#ifndef TRANSACTION_MANAGER_H
#define TRANSACTION_MANAGER_H

#include "kvdb.h"
#include "lock_manager.h"
#include <memory>

class TransactionManager
{
public:
    explicit TransactionManager(SessionId session_id);
    ~TransactionManager();

    std::string begin(IsolationLevel isolation_level = IsolationLevel::REPEATABLE_READ);

    std::string commit();

    std::string abort();

    std::string put(const Key &key, const Value &value);

    std::string delete_key(const Key &key);

    std::string get(const Key &key, Value &out_value);

    bool isInTransaction() const;

    TxnId getCurrentTxnId() const;

    IsolationLevel getIsolationLevel() const;

    std::string getStats() const;

    std::vector<Key> getHeldLocks() const;

private:
    SessionId session_id_;
    TransactionInfo current_txn_;
    TxnId next_txn_id_ = 1;

    uint64_t total_commits_ = 0;
    uint64_t deadlock_aborts_ = 0;

    void applyPendingBuffer();
};

#endif // TRANSACTION_MANAGER_H
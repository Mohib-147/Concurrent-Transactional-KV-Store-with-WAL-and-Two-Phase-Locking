#ifndef LOCK_MANAGER_H
#define LOCK_MANAGER_H

#include "kvdb.h"
#include <map>
#include <mutex>
#include <memory>

class LockManager
{
public:
    LockManager();
    ~LockManager();

    bool acquireLock(TxnId txn_id, const Key &key, LockMode mode, int timeout_ms = 3000);

    void releaseAllLocks(TxnId txn_id);

    void releaseLock(TxnId txn_id, const Key &key);

    LockMode hasLock(TxnId txn_id, const Key &key) const;

    std::string dumpLockTable() const;

    size_t getLockedKeyCount() const;

private:
    std::map<Key, LockEntry> lock_table_;

    mutable std::mutex lock_table_mutex_;

    static bool isCompatible(LockMode mode1, LockMode mode2);

    void grantLock(TxnId txn_id, Key &key, LockMode mode);

    void processWaiters(const Key &key);
};

#endif // LOCK_MANAGER_H
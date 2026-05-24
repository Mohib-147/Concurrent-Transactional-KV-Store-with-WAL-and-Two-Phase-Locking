#include "../include/kvdb.h"
#include "../src/core/lock_manager.h"
#include <cassert>
#include <iostream>

int main()
{
    LockManager lm;

    assert(lm.acquireLock(1, "key1", LockMode::SHARED));
    assert(lm.acquireLock(2, "key1", LockMode::SHARED));
    assert(lm.hasLock(1, "key1") == LockMode::SHARED);

    lm.releaseAllLocks(1);
    lm.releaseAllLocks(2);
    assert(lm.acquireLock(3, "key2", LockMode::EXCLUSIVE));
    assert(lm.hasLock(3, "key2") == LockMode::EXCLUSIVE);
    lm.releaseAllLocks(3);

    std::cout << "test_lock_manager: all assertions passed\n";
    return 0;
}

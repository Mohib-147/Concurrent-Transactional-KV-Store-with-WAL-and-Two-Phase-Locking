#ifndef SERVER_H
#define SERVER_H

#include "kvdb.h"
#include "../core/lock_manager.h"
#include "../core/store.h"
#include "../core/wal_manager.h"
#include "../core/group_commit.h"
#include "../core/checkpoint_manager.h"

#include <memory>
#include <map>
#include <thread>
#include <mutex>
#include <atomic>

class TransactionManager;

class Server : public std::enable_shared_from_this<Server>
{
public:
    explicit Server(int port);
    ~Server();

    void start();

    void shutdown();

    std::shared_ptr<Store> getStore() const
    {
        return store_;
    }

    std::shared_ptr<LockManager> getLockManager() const
    {
        return lock_manager_;
    }

    std::shared_ptr<TransactionManager>
    getTransactionManager(SessionId session_id);

    TxnId allocateTxnId();

private:
    int port_;
    int server_socket_ = -1;
    bool running_ = false;

    std::shared_ptr<Store> store_;
    std::shared_ptr<LockManager> lock_manager_;
    std::shared_ptr<WALManager> wal_manager_;
    std::shared_ptr<GroupCommitManager> group_commit_manager_;

    std::map<SessionId, std::shared_ptr<TransactionManager>> txn_managers_;
    std::mutex txn_managers_mutex_;

    SessionId next_session_id_ = 1;

    std::atomic<TxnId> next_txn_id_{1};

    void acceptLoop();

    bool hasActiveTransactions() const;

    bool handleCheckpoint();

    std::shared_ptr<CheckpointManager> checkpoint_manager_;

public:
    std::shared_ptr<WALManager> getWALManager() const { return wal_manager_; }
    std::shared_ptr<GroupCommitManager> getGroupCommitManager() const { return group_commit_manager_; }
    void initAfterConstruction();
};

#endif
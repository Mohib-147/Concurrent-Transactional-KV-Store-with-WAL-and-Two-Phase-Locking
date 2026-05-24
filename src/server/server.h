#ifndef SERVER_H
#define SERVER_H

#include "kvdb.h"
#include "../core/lock_manager.h"
#include "../core/store.h"
#include <memory>
#include <map>
#include <thread>
#include <mutex>

class TransactionManager;

class Server : public std::enable_shared_from_this<Server>
{
public:
    explicit Server(int port);
    ~Server();

    void start();

    void shutdown();

    std::shared_ptr<Store> getStore() const { return store_; }

    std::shared_ptr<LockManager> getLockManager() const { return lock_manager_; }

    std::shared_ptr<TransactionManager> getTransactionManager(SessionId session_id);

private:
    int port_;
    int server_socket_ = -1;
    bool running_ = false;

    std::shared_ptr<Store> store_;
    std::shared_ptr<LockManager> lock_manager_;

    std::map<SessionId, std::shared_ptr<TransactionManager>> txn_managers_;
    std::mutex txn_managers_mutex_;
    SessionId next_session_id_ = 1;

    void acceptLoop();
};

#endif
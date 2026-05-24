#include "server.h"
#include "../core/lock_manager.h"
#include "../core/store.h"
#include "../core/transaction_manager.h"
#include "connection_handler.h"
#include <iostream>
#include <thread>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>

Server::Server(int port) : port_(port)
{
    // Initialize shared resources
    store_ = std::make_shared<Store>();
    lock_manager_ = std::make_shared<LockManager>();
}

Server::~Server()
{
    shutdown();
}

void Server::start()
{
    std::cout << "[server] starting up..." << std::endl;

    running_ = true;

    // Create server socket
    server_socket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket_ < 0)
    {
        std::cerr << "[server] ERROR: Failed to create socket" << std::endl;
        return;
    }

    // Set socket options (allow reuse of address)
    int opt = 1;
    if (setsockopt(server_socket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        std::cerr << "[server] ERROR: Failed to set socket options" << std::endl;
        close(server_socket_);
        return;
    }

    // Bind to port
    sockaddr_in server_addr;
    std::memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port_);

    if (bind(server_socket_, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        std::cerr << "[server] ERROR: Failed to bind to port " << port_ << std::endl;
        close(server_socket_);
        return;
    }

    // Listen for connections
    if (listen(server_socket_, 5) < 0)
    {
        std::cerr << "[server] ERROR: Failed to listen" << std::endl;
        close(server_socket_);
        return;
    }

    std::cout << "[server] listening on port " << port_ << std::endl;
    std::cout << "[server] lock manager initialized" << std::endl;
    std::cout << "[server] ready to accept connections" << std::endl;

    // Accept loop — must be called on a shared_ptr-managed Server instance
    // so that shared_from_this() works inside acceptLoop.
    acceptLoop();
}

void Server::acceptLoop()
{
    while (running_)
    {
        sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);

        // Accept incoming connection
        int client_socket = accept(server_socket_,
                                   (struct sockaddr *)&client_addr,
                                   &client_addr_len);

        if (client_socket < 0)
        {
            if (running_)
            {
                std::cerr << "[server] ERROR: Failed to accept connection" << std::endl;
            }
            break;
        }

        // Assign session ID
        SessionId session_id;
        {
            std::unique_lock<std::mutex> lock(txn_managers_mutex_);
            session_id = next_session_id_++;
        }

        // Get transaction manager for this session
        auto txn_mgr = getTransactionManager(session_id);

        // Spawn thread to handle connection
        auto handler = std::make_shared<ConnectionHandler>(
            client_socket,
            session_id,
            shared_from_this());

        std::thread client_thread([handler]()
                                  { handler->run(); });

        client_thread.detach();
    }
}

std::shared_ptr<TransactionManager> Server::getTransactionManager(SessionId session_id)
{
    std::unique_lock<std::mutex> lock(txn_managers_mutex_);

    auto it = txn_managers_.find(session_id);
    if (it != txn_managers_.end())
    {
        return it->second;
    }

    // Create new transaction manager for this session
    auto txn_mgr = std::make_shared<TransactionManager>(session_id);
    txn_mgr->setGlobalReferences(store_, lock_manager_);
    txn_managers_[session_id] = txn_mgr;

    return txn_mgr;
}

void Server::shutdown()
{
    running_ = false;

    if (server_socket_ >= 0)
    {
        close(server_socket_);
        server_socket_ = -1;
    }
}
#ifndef CONNECTION_HANDLER_H
#define CONNECTION_HANDLER_H

#include "kvdb.h"
#include <memory>

class Server;
class TransactionManager;

class ConnectionHandler
{
public:
    explicit ConnectionHandler(int client_socket, SessionId session_id, std::shared_ptr<Server> server);

    ~ConnectionHandler();

    void run();

private:
    int client_socket_;
    SessionId session_id_;
    std::shared_ptr<Server> server_;
    std::shared_ptr<TransactionManager> txn_manager_;

    std::string readLine();

    void writeLine(const std::string &response);

    std::string handleCommand(const std::string &cmd_line);

    void closeConnection();
};

#endif
#include "connection_handler.h"
#include "server.h"
#include "../core/transaction_manager.h"
#include "../protocol/parser.h"
#include "../protocol/protocol.h"
#include <iostream>
#include <sstream>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>

ConnectionHandler::ConnectionHandler(int client_socket, SessionId session_id,
                                     std::shared_ptr<Server> server)
    : client_socket_(client_socket), session_id_(session_id), server_(server)
{
    txn_manager_ = server_->getTransactionManager(session_id);
    inbuf_.clear();
}

ConnectionHandler::~ConnectionHandler()
{
    closeConnection();
}

void ConnectionHandler::run()
{
    std::string greeting = Protocol::getGreeting(session_id_);
    writeLine(greeting);

    std::cout << "[server] client connected: session " << session_id_ << std::endl;

    std::string cmd;
    while (getNextCommandLine(cmd))
    {
        if (cmd.empty())
            continue;

        std::string response = handleCommand(cmd);

        if (response.empty())
            break;

        writeLine(response);
    }

    std::cout << "[server] client disconnected: session " << session_id_ << std::endl;
    closeConnection();
}

bool ConnectionHandler::getNextCommandLine(std::string &cmd_line)
{
    cmd_line.clear();

    while (true)
    {
        auto newline_pos = inbuf_.find('\n');
        if (newline_pos != std::string::npos)
        {
            cmd_line = inbuf_.substr(0, newline_pos);
            inbuf_ = inbuf_.substr(newline_pos + 1);

            if (!cmd_line.empty() && cmd_line.back() == '\r')
                cmd_line.pop_back();

            return true;
        }

        char buf[1024];
        ssize_t n = recv(client_socket_, buf, sizeof(buf), 0);
        if (n == 0)
        {
            return false;
        }
        else if (n < 0)
        {
            return false;
        }
        inbuf_.append(buf, n);
    }
}

void ConnectionHandler::writeLine(const std::string &response)
{
    std::string msg = response + "\n";
    send(client_socket_, msg.c_str(), msg.length(), 0);
}

std::string ConnectionHandler::handleCommand(const std::string &cmd_line)
{
    // Parse command
    CommandParser::Command cmd = CommandParser::parse(cmd_line);

    if (cmd.name.empty())
    {
        return "ERROR: Empty command";
    }

    // Dispatch to handlers
    if (cmd.name == "BEGIN")
    {
        IsolationLevel level = IsolationLevel::REPEATABLE_READ;
        if (!cmd.args.empty())
        {
            level = parseIsolationLevel(cmd.args[0]);
        }

        std::string result = txn_manager_->begin(level);
        if (result.empty())
        {
            return "txn " + std::to_string(txn_manager_->getCurrentTxnId()) + " started.";
        }
        else
        {
            return "ERROR: " + result;
        }
    }

    else if (cmd.name == "PUT")
    {
        if (cmd.args.size() < 2)
        {
            return "ERROR: PUT requires key and value";
        }

        Key key = cmd.args[0];
        Value value = cmd.args[1];

        std::string result = txn_manager_->put(key, value);
        if (result.empty())
        {
            return "OK";
        }
        else
        {
            return "ERROR: " + result;
        }
    }

    else if (cmd.name == "GET")
    {
        if (cmd.args.size() < 1)
        {
            return "ERROR: GET requires key";
        }

        Key key = cmd.args[0];
        Value value;

        std::string result = txn_manager_->get(key, value);
        if (result.empty())
        {
            return value;
        }
        else
        {
            return "ERROR: " + result;
        }
    }

    else if (cmd.name == "DELETE")
    {
        if (cmd.args.size() < 1)
        {
            return "ERROR: DELETE requires key";
        }

        Key key = cmd.args[0];

        std::string result = txn_manager_->delete_key(key);
        if (result.empty())
        {
            return "OK";
        }
        else
        {
            return "ERROR: " + result;
        }
    }

    else if (cmd.name == "COMMIT")
    {
        TxnId committed_txn_id = txn_manager_->getCurrentTxnId();
        std::string result = txn_manager_->commit();
        if (result.empty())
        {
            return "txn " + std::to_string(committed_txn_id) + " committed.";
        }
        else
        {
            return "ERROR: " + result;
        }
    }

    else if (cmd.name == "ABORT")
    {
        std::string result = txn_manager_->abort();
        if (result.empty())
        {
            return "OK";
        }
        else
        {
            return "ERROR: " + result;
        }
    }

    else if (cmd.name == "\\STATS")
    {
        return txn_manager_->getStats();
    }

    else if (cmd.name == "\\LOCKS")
    {
        return server_->getLockManager()->dumpLockTable();
    }

    else if (cmd.name == "CRASH")
    {
        std::cout << "[server] CRASH command received, exiting" << std::endl;
        _exit(1);
    }

    else if (cmd.name == "QUIT")
    {
        // Abort any active transaction
        if (txn_manager_->isInTransaction())
        {
            txn_manager_->abort();
        }
        return ""; // Signal connection close
    }
    else if (cmd.name == "CHECKPOINT")
    {
        std::cout << "[server] CHECKPOINT requested (session "
                  << session_id_ << ")" << std::endl;

        bool ok = server_->handleCheckpoint();

        if (ok)
        {
            return "CHECKPOINT OK";
        }
        else
        {
            return "ERROR: checkpoint refused (active transactions exist)";
        }
    }

    else
    {
        return "ERROR: Unknown command: " + cmd.name;
    }
}

void ConnectionHandler::closeConnection()
{
    if (client_socket_ >= 0)
    {
        close(client_socket_);
        client_socket_ = -1;
    }
}
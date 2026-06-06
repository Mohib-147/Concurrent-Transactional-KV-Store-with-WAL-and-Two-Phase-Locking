#ifndef CLIENT_H
#define CLIENT_H

#include <string>
#include <cstdint>

class Client
{
public:
    Client(const std::string &host, int port);
    ~Client();

    bool connectToServer();
    void disconnect();

    // transaction control
    std::string begin(const std::string &isolation = "");
    std::string commit();
    std::string abort();

    // operations
    std::string put(const std::string &key, const std::string &value);
    std::string del(const std::string &key);
    std::string get(const std::string &key, std::string &value_out);
    std::string sendCommand(const std::string &command);

    bool isConnected() const;

private:
    int sock_;
    std::string host_;
    int port_;

    bool sendLine(const std::string &msg);
    bool readLine(std::string &out);

    bool request(const std::string &cmd, std::string *response);
};
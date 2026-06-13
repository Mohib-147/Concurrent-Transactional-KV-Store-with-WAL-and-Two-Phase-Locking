#ifndef CLIENT_H
#define CLIENT_H

#include <string>
#include <memory>

class Client
{
public:
    explicit Client(const std::string &host, int port);

    ~Client();

    bool isConnected() const { return socket_ >= 0; }

    std::string sendCommand(const std::string &command);
    std::string receiveLine();

    void close();

private:
    std::string host_;
    int port_;
    int socket_ = -1;

    bool connect();

    std::string readLine();
};

#endif // CLIENT_H
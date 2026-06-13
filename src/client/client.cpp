#include "client.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <cstring>
#include <iostream>

Client::Client(const std::string &host, int port)
    : host_(host), port_(port), socket_(-1)
{
    connect();
}

Client::~Client()
{
    close();
}

bool Client::connect()
{
    socket_ = -1;

    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    addrinfo *result = nullptr;
    const std::string port_str = std::to_string(port_);

    int rc = getaddrinfo(host_.c_str(), port_str.c_str(), &hints, &result);
    if (rc != 0)
    {
        std::cerr << "ERROR: Failed to resolve host '" << host_ << "': "
                  << gai_strerror(rc) << std::endl;
        return false;
    }

    for (addrinfo *rp = result; rp != nullptr; rp = rp->ai_next)
    {
        int sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sock < 0)
        {
            continue;
        }

        if (::connect(sock, rp->ai_addr, rp->ai_addrlen) == 0)
        {
            socket_ = sock;
            break;
        }

        ::close(sock);
    }

    freeaddrinfo(result);

    if (socket_ < 0)
    {
        std::cerr << "ERROR: Failed to connect to " << host_ << ":" << port_ << std::endl;
        return false;
    }

    return true;
}

std::string Client::sendCommand(const std::string &command)
{
    if (socket_ < 0)
    {
        return "ERROR: Not connected";
    }

    // Send command
    std::string msg = command + "\n";
    ssize_t sent = send(socket_, msg.c_str(), msg.length(), MSG_NOSIGNAL);
    if (sent < 0)
    {
        return "ERROR: Failed to send";
    }

    // Receive response
    return readLine();
}

std::string Client::receiveLine()
{
    if (socket_ < 0)
    {
        return "";
    }
    return readLine();
}

std::string Client::readLine()
{
    char buffer[4096];
    ssize_t bytes_read = recv(socket_, buffer, sizeof(buffer) - 1, 0);

    if (bytes_read < 0)
    {
        return "ERROR: Failed to receive";
    }

    if (bytes_read == 0)
    {
        return ""; // Connection closed
    }

    buffer[bytes_read] = '\0';
    std::string response(buffer);

    // Remove newline
    if (!response.empty() && response.back() == '\n')
    {
        response.pop_back();
    }
    if (!response.empty() && response.back() == '\r')
    {
        response.pop_back();
    }

    return response;
}

void Client::close()
{
    if (socket_ >= 0)
    {
        ::close(socket_);
        socket_ = -1;
    }
}
#include "client.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
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
    socket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_ < 0)
    {
        std::cerr << "ERROR: Failed to create socket" << std::endl;
        return false;
    }

    sockaddr_in server_addr;
    std::memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port_);

    if (inet_pton(AF_INET, host_.c_str(), &server_addr.sin_addr) <= 0)
    {
        std::cerr << "ERROR: Invalid address" << std::endl;
        return false;
    }

    if (::connect(socket_, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
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
    ssize_t sent = send(socket_, msg.c_str(), msg.length(), 0);
    if (sent < 0)
    {
        return "ERROR: Failed to send";
    }

    // Receive response
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
#include "client.h"

#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>

// ---------------- constructor ----------------

Client::Client(const std::string &host, int port)
    : host_(host), port_(port), sock_(-1)
{
}

// ---------------- destructor ----------------

Client::~Client()
{
    disconnect();
}

// ---------------- connection ----------------

bool Client::connectToServer()
{
    sock_ = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_ < 0)
        return false;

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port_);

    if (inet_pton(AF_INET, host_.c_str(), &addr.sin_addr) <= 0)
        return false;

    if (::connect(sock_, (sockaddr *)&addr, sizeof(addr)) < 0)
        return false;

    return true;
}

void Client::disconnect()
{
    if (sock_ >= 0)
    {
        close(sock_);
        sock_ = -1;
    }
}

bool Client::isConnected() const
{
    return sock_ >= 0;
}

// ---------------- low-level IO ----------------

bool Client::sendLine(const std::string &msg)
{
    std::string data = msg + "\n";

    size_t sent = 0;
    while (sent < data.size())
    {
        ssize_t n = send(sock_, data.data() + sent, data.size() - sent, 0);
        if (n <= 0)
            return false;
        sent += n;
    }

    return true;
}

bool Client::readLine(std::string &out)
{
    out.clear();

    char c;
    while (true)
    {
        ssize_t n = recv(sock_, &c, 1, 0);

        if (n <= 0)
            return false;

        if (c == '\n')
            break;

        if (c != '\r')
            out.push_back(c);
    }

    return true;
}

bool Client::request(const std::string &cmd, std::string *response)
{
    if (!sendLine(cmd))
        return false;

    if (response)
        return readLine(*response);

    std::string dummy;
    return readLine(dummy);
}

// ---------------- API ----------------

std::string Client::begin(const std::string &isolation)
{
    std::string resp;
    if (isolation.empty())
        request("BEGIN", &resp);
    else
        request("BEGIN " + isolation, &resp);
    return resp;
}

std::string Client::commit()
{
    std::string resp;
    request("COMMIT", &resp);
    return resp;
}

std::string Client::abort()
{
    std::string resp;
    request("ABORT", &resp);
    return resp;
}

std::string Client::put(const std::string &key, const std::string &value)
{
    std::string resp;
    request("PUT " + key + " " + value, &resp);
    return resp;
}

std::string Client::del(const std::string &key)
{
    std::string resp;
    request("DELETE " + key, &resp);
    return resp;
}

std::string Client::get(const std::string &key, std::string &value_out)
{
    std::string resp;
    request("GET " + key, &resp);
    value_out = resp;
    return resp;
}

std::string Client::sendCommand(const std::string &command)
{
    std::string resp;
    request(command, &resp);
    return resp;
}
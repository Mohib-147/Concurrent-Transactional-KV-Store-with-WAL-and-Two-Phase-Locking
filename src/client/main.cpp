#include "client.h"
#include <iostream>
#include <string>
#include <algorithm>

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        std::cout << "Usage: " << argv[0] << " <host> <port>" << std::endl;
        return 1;
    }

    std::string host = argv[1];
    int port = std::stoi(argv[2]);

    // Connect to server
    Client client(host, port);

    if (!client.isConnected())
    {
        std::cerr << "Failed to connect to server" << std::endl;
        return 1;
    }

    std::string greeting = client.receiveLine();
    if (!greeting.empty())
    {
        std::cout << greeting << std::endl;
    }

    std::string line;
    std::cout << "> ";

    while (std::getline(std::cin, line))
    {
        if (line.empty())
        {
            std::cout << "> ";
            continue;
        }

        std::string upper = line;
        std::transform(upper.begin(), upper.end(), upper.begin(),
                       [](unsigned char c)
                       { return static_cast<char>(std::toupper(c)); });

        if (upper == "QUIT;" || upper == "QUIT")
        {
            std::cout << "Disconnecting..." << std::endl;
            break;
        }

        std::string response = client.sendCommand(line);
        if (response.empty())
        {
            std::cout << "Server closed connection." << std::endl;
            break;
        }
        std::cout << response << std::endl;
        std::cout << "> ";
    }

    return 0;
}
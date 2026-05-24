#include "client.h"
#include <iostream>
#include <string>

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

    std::string line;
    std::cout << "> ";

    while (std::getline(std::cin, line))
    {
        if (line.empty())
        {
            std::cout << "> ";
            continue;
        }

        if (line == "QUIT;" || line == "QUIT")
        {
            std::cout << "Disconnecting..." << std::endl;
            break;
        }

        std::string response = client.sendCommand(line);
        std::cout << response << std::endl;
        std::cout << "> ";
    }

    return 0;
}
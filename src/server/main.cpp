#include "server.h"
#include <iostream>
#include <cstring>
#include <memory>

void printUsage(const char *program)
{
    std::cout << "Usage: " << program << " --port <port>" << std::endl;
    std::cout << "  --port <port>  : TCP port to listen on (default: 7000)" << std::endl;
}

int main(int argc, char *argv[])
{
    int port = 7000;

    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--port") == 0)
        {
            if (i + 1 < argc)
            {
                port = std::stoi(argv[++i]);
            }
            else
            {
                std::cerr << "ERROR: --port requires an argument" << std::endl;
                return 1;
            }
        }
        else
        {
            printUsage(argv[0]);
            return 1;
        }
    }

    std::cout << "[server] starting up..." << std::endl;

    auto server = std::make_shared<Server>(port);
    server->start();

    return 0;
}
#ifndef PARSER_H
#define PARSER_H

#include <string>
#include <vector>

class CommandParser
{
public:
    struct Command
    {
        std::string name;
        std::vector<std::string> args;
    };

    static Command parse(const std::string &line);

    static std::string trim(const std::string &str);
};

#endif // PARSER_H
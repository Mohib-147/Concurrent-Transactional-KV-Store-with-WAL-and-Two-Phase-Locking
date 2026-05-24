#include "../protocol/parser.h"
#include <sstream>
#include <algorithm>
#include <cctype>

std::string CommandParser::trim(const std::string &str)
{
    auto start = str.begin();
    while (start != str.end() && std::isspace(*start))
    {
        ++start;
    }

    auto end = str.end();
    do
    {
        --end;
    } while (std::distance(start, end) > 0 && std::isspace(*end));

    return std::string(start, end + 1);
}

CommandParser::Command CommandParser::parse(const std::string &line)
{
    Command cmd;

    std::string trimmed = trim(line);

    if (!trimmed.empty() && trimmed.back() == ';')
    {
        trimmed.pop_back();
    }

    std::istringstream iss(trimmed);
    std::string token;

    bool is_first = true;
    while (iss >> token)
    {
        if (is_first)
        {
            std::transform(token.begin(), token.end(), token.begin(), ::toupper);
            cmd.name = token;
            is_first = false;
        }
        else
        {
            cmd.args.push_back(token);
        }
    }

    return cmd;
}
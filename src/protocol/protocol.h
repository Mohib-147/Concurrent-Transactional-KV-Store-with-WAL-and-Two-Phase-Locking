#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <string>
#include <cstdint>

namespace Protocol
{
    constexpr const char *CMD_BEGIN = "BEGIN";
    constexpr const char *CMD_PUT = "PUT";
    constexpr const char *CMD_GET = "GET";
    constexpr const char *CMD_DELETE = "DELETE";
    constexpr const char *CMD_COMMIT = "COMMIT";
    constexpr const char *CMD_ABORT = "ABORT";
    constexpr const char *CMD_CHECKPOINT = "CHECKPOINT";
    constexpr const char *CMD_STATS = "\\stats";
    constexpr const char *CMD_LOCKS = "\\locks";
    constexpr const char *CMD_CRASH = "CRASH";
    constexpr const char *CMD_QUIT = "QUIT";

    constexpr const char *RESP_OK = "OK";
    constexpr const char *RESP_ERROR = "ERROR";
    constexpr const char *RESP_BLOCKED = "BLOCKED";

    std::string getGreeting(uint32_t session_id);

}

#endif
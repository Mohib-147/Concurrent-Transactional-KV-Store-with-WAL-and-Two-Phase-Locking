#include "../protocol/protocol.h"

namespace Protocol
{

    std::string getGreeting(uint32_t session_id)
    {
        return "KVDB ready (session " + std::to_string(session_id) + ")";
    }

}
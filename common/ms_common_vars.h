#ifndef MS_COMMON_VARS_H
#define MS_COMMON_VARS_H

#include "ms_common_types.h"

namespace common_vars {

static constexpr common_types::TContextId INVALID_CONTEXT_ID = 0;
static constexpr common_types::TMissionId INVALID_MISSION_ID = 0;

namespace cmd {
static const std::string COMMAND_TYPE = "cmd_type"; // analyze
static const std::string COMMAND_NAME = "cmd_name"; // player
static const std::string COMMAND_ACTION = "cmd_action"; // start

static const std::string USER_ID = "user_id";
static const std::string USER_IP = "user_ip";
static const std::string USER_PID = "user_pid";
static const std::string CONTEXT_NAME = "context_name";

}

namespace mongo_fields {
namespace wal_client_operations {
    const std::string COLLECTION_NAME = "wal_client_operations";

    const std::string BEGIN = "begin";
    const std::string TYPE = "type";
    const std::string UNIQUE_KEY = "unique_key";
    const std::string FULL_TEXT = "full_text";
}

namespace wal_process_events {
    const std::string COLLECTION_NAME = "wal_process_events";

    const std::string PID = "pid";
    const std::string LAUNCHED = "launched";
    const std::string PROGRAM_NAME = "program_name";
    const std::string PROGRAM_ARGS = "program_args";
}
}

}

#endif // MS_COMMON_VARS_H

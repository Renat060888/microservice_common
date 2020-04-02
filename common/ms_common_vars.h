#ifndef MS_COMMON_VARS_H
#define MS_COMMON_VARS_H

#include "ms_common_types.h"

namespace common_vars {

static constexpr common_types::TContextId INVALID_CONTEXT_ID = 0;
static constexpr common_types::TMissionId INVALID_MISSION_ID = 0;
static constexpr common_types::TSessionNum INVALID_SESSION_NUM = -1;
static constexpr common_types::TLogicStep INVALID_LOGIC_STEP = -1;

namespace cmd {
static const std::string COMMAND_TYPE = "cmd_type"; // analyze
static const std::string COMMAND_NAME = "cmd_name"; // player
static const std::string COMMAND_ACTION = "cmd_action"; // start

static const std::string USER_ID = "user_id";
static const std::string USER_IP = "user_ip";
static const std::string USER_PID = "user_pid";
static const std::string CONTEXT_NAME = "ctx_name";

}

namespace mongo_fields {

namespace analytic {

namespace metadata {
    const std::string COLLECTION_NAME = "analytic_metadata";
    const std::string CTX_ID = "ctx_id";
    const std::string SENSOR_ID = "sensor_id";
    const std::string LAST_SESSION_ID = "last_session_id";
    const std::string UPDATE_STEP_MILLISEC = "update_step_millisec";
}
    const std::string COLLECTION_NAME = "analytic_events";

    const std::string JSON = "json";
    const std::string WRITED_AT_TIME = "writed_at_time";
    const std::string NAME_PLUGIN = "plugin_name";
    const std::string NAME_SOURCE = "source_name";
    const std::string TYPE_PLUGIN = "plugin_type";

namespace detected_object {
    const std::string OBJRERP_ID = "objrepr_id";
    const std::string LAT = "lat";
    const std::string LON = "lon";
    const std::string YAW = "yaw";
    const std::string STATE = "state";
    const std::string ASTRO_TIME = "astro_time";
    const std::string LOGIC_TIME = "logic_time";
    const std::string SESSION = "session";
}

}

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

#ifndef MS_COMMON_VARS_H
#define MS_COMMON_VARS_H

#include <limits>

#include "ms_common_types.h"

namespace common_vars {

// ------------------------------------------------------------------------
// constants
// ------------------------------------------------------------------------
static constexpr common_types::TContextId ALL_CONTEXT_ID = std::numeric_limits<common_types::TContextId>::max();
static constexpr common_types::TContextId INVALID_CONTEXT_ID = 0;
static constexpr common_types::TSessionNum INVALID_SESSION_NUM = -1;
static constexpr common_types::TPersistenceSetId INVALID_PERS_ID = -1;
static constexpr common_types::TSessionNum ALL_SESSION_NUM = std::numeric_limits<common_types::TSessionNum>::max();
static constexpr common_types::TMissionId INVALID_MISSION_ID = 0;
static constexpr common_types::TLogicStep INVALID_LOGIC_STEP = -1;
static constexpr common_types::TLogicStep ALL_LOGIC_STEPS = std::numeric_limits<common_types::TLogicStep>::min();

// ------------------------------------------------------------------------
// network commands
// ------------------------------------------------------------------------
namespace cmd {
static const std::string COMMAND_TYPE = "cmd_type"; // analyze
static const std::string COMMAND_NAME = "cmd_name"; // player
static const std::string COMMAND_ACTION = "cmd_action"; // start

static const std::string USER_ID = "user_id";
static const std::string USER_IP = "user_ip";
static const std::string USER_PID = "user_pid";
static const std::string CONTEXT_NAME = "context_name";
static const std::string MISSION_NAME = "mission_name";

}

// ------------------------------------------------------------------------
// database fields
// ------------------------------------------------------------------------
namespace mongo_fields {
namespace analytic {
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
    const std::string HEIGHT = "height";
    const std::string YAW = "yaw";
    const std::string STATE = "state";
    const std::string ASTRO_TIME = "astro_time";
    const std::string LOGIC_TIME = "logic_time";
    const std::string SESSION = "session";
}

}

// metadata
namespace persistence_set_metadata {
    const std::string COLLECTION_NAME = "persistence_set_metadata";

    const std::string PERSISTENCE_ID = "persistence_id";
    const std::string CTX_ID = "ctx_id";
    const std::string MISSION_ID = "mission_id";
    const std::string LAST_SESSION_ID = "last_session_id";
    const std::string UPDATE_STEP_MILLISEC = "update_step_millisec";
    const std::string SOURCE_TYPE = "source_type";
    const std::string DATA_TYPE = "data_type";
    const std::string PAYLOAD_TABLE_NAME = "payload_table_name";
}

namespace persistence_set_metadata_video {
    const std::string COLLECTION_NAME = "persistence_set_metadata_video";

    const std::string SENSOR_ID = "sensor_id";
}

namespace persistence_set_metadata_raw {
    const std::string COLLECTION_NAME = "persistence_set_metadata_raw";

    const std::string SENSOR_ID = "sensor_id";
}

namespace persistence_set_metadata_dss {
    const std::string COLLECTION_NAME = "persistence_set_metadata_dss";

    const std::string REAL = "real";
}

// description
namespace persistence_set_description {
    const std::string COLLECTION_NAME = "persistence_set_description";

    const std::string PERSISTENCE_ID = "persistence_id";
    const std::string SESSION_NUM = "session_num";
    const std::string LOGIC_TIME_MAX = "logic_time_max";
    const std::string LOGIC_TIME_MIN = "logic_time_min";
    const std::string ASTRO_TIME_MAX = "astro_time_max";
    const std::string ASTRO_TIME_MIN = "astro_time_min";
    const std::string EMPTY_STEPS_BEGIN = "empty_steps_begin";
    const std::string EMPTY_STEPS_END = "empty_steps_end";
}

// wal
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

namespace wal_user_registrations {
    const std::string COLLECTION_NAME = "wal_user_registrations";

    const std::string USER_ID = "user_id";
    const std::string USER_IP = "user_ip";
    const std::string USER_PID = "user_pid";
    const std::string REGISTERED_AT_TIME_MILLISEC = "registered_at_time_millisec";
}

}

}

#endif // MS_COMMON_VARS_H

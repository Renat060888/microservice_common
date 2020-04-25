
#include <iostream>
#include <fstream>
#include <dirent.h>

#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/property_tree/json_parser.hpp>

#include "a_config_reader.h"
#include "common/ms_common_types.h"

using namespace std;

static string g_default_main_config_file_name;
const char * AConfigReader::PRINT_HEADER = "ConfigReader:";

AConfigReader::AConfigReader()
{

}

AConfigReader::~AConfigReader()
{

}

bool AConfigReader::init( const SIninSettings & _settings ){

    assert( ! _settings.projectName.empty() );

    m_settings = _settings;
    m_commandConvertor = _settings.commandConvertor;

    g_default_main_config_file_name = _settings.projectName + "_main_cfg.json";

    // get content
    const std::string mainConfigFile = getConfigFilePath();
    if( mainConfigFile.empty() ){
        PRELOG_ERR << PRINT_HEADER << " config file not found" << endl;
        return false;
    }

    PRELOG_INFO << PRINT_HEADER << " read config from [" << mainConfigFile << "]" << endl;

    ifstream inputFile( mainConfigFile.c_str() );
    const string content = string( std::istreambuf_iterator<char>(inputFile),
                                   std::istreambuf_iterator<char>() );

    // parse
    if( ! parseBase(content) ){
        return false;
    }

    if( ! isParametersCorrect() ){
        return false;
    }

    // relative path to absolute
    const string path = mainConfigFile.substr( 0, mainConfigFile.find_last_of("/") );
    m_parameters.OBJREPR_CONFIG_PATH = path + "/" + m_parameters.OBJREPR_CONFIG_PATH;
    m_parameters.SYSTEM_UNILOG_CONFIG_PATH = path + "/" + m_parameters.SYSTEM_UNILOG_CONFIG_PATH;

    if( ! createCommandsFromConfig(content) ){
        return false;
    }

    if( ! initDerive(_settings) ){
        return false;
    }

    return true;
}

std::string AConfigReader::getConfigExample(){

    // TODO: print base parameters

    const string deriveExample = getConfigExampleDerive();
    return deriveExample;
}

bool AConfigReader::parseBase( const string & _content ){

    // parse base part
    boost::property_tree::ptree config;

    istringstream contentStream( _content );
    try{
        boost::property_tree::json_parser::read_json( contentStream, config );
    }
    catch( boost::property_tree::json_parser::json_parser_error & _ex ){
        PRELOG_ERR << PRINT_HEADER << " parse failed of [" << _content << "]" << endl
             << "Reason: [" << _ex.what() << "]" << endl;
        return false;
    }

    boost::property_tree::ptree system = config.get_child("system");


    m_parameters.SYSTEM_CONNECT_RETRIES = setParameterNew<int32_t>( system, "connect_retries", 3 );
    m_parameters.SYSTEM_CONNECT_RETRY_PERIOD_SEC = setParameterNew<int32_t>( system, "connect_retry_period_sec", 5 );
    m_parameters.SYSTEM_UNILOG_CONFIG_PATH = setParameterNew<std::string>( system, "unilog_config_path", string("unilog_cfg.xml") );
    m_parameters.SYSTEM_LOG_TO_STDOUT = setParameterNew<bool>( system, "log_to_stdout", true );
    m_parameters.SYSTEM_LOG_TO_FILE = setParameterNew<bool>( system, "log_to_file", false );
    m_parameters.SYSTEM_LOGFILE_NAME = setParameterNew<std::string>( system, "log_file_name", string("video_server") );
    m_parameters.SYSTEM_REGULAR_LOGFILE_PATH = setParameterNew<std::string>( system, "regular_logfile_path", string("/tmp") );
    m_parameters.SYSTEM_CRUSH_LOGFILE_PATH = setParameterNew<std::string>( system, "crush_logfile_path", string("/tmp") );
    m_parameters.SYSTEM_LOGFILE_ROTATION_SIZE_MB = setParameterNew<int32_t>( system, "logfile_rotation_size_mb", 1 );
    m_parameters.SYSTEM_UNIQUE_ID = setParameterNew<std::string>( system, "unique_id", string("video_server_unique_id") );
    m_parameters.SYSTEM_CATCH_CHILD_PROCESSES_OUTPUT = setParameterNew<bool>( system, "catch_child_processes_output", false );
    const int selfShutdownAfterHour = setParameterNew<int32_t>( system, "self_shutdown_after_hour", 0 );
    const int selfShutdownAfterMin = setParameterNew<int32_t>( system, "self_shutdown_after_min", 0 );
    const int selfShutdownAfterSec = setParameterNew<int32_t>( system, "self_shutdown_after_sec", 0 );
    m_parameters.SYSTEM_SELF_SHUTDOWN_SEC = selfShutdownAfterSec + (selfShutdownAfterMin * 60) + (selfShutdownAfterHour * 60 * 60);
    m_parameters.SYSTEM_RESTORE_INTERRUPTED_SESSION = setParameterNew<bool>( system, "restore_interrupted_session", false );

    static const string FEATURES_DELIMETER = "|";
    string serverFeaturesStr = setParameterNew<std::string>( system, "server_features", string("archiving | analyze") );
    serverFeaturesStr.erase( std::remove( serverFeaturesStr.begin(), serverFeaturesStr.end(), ' ' ),
                             serverFeaturesStr.end() );
    std::vector<string> out;
    boost::algorithm::split( out, serverFeaturesStr, boost::is_any_of(FEATURES_DELIMETER) );
    for( const string & feature : out ){
        if( "retranslation" == feature ){
            m_parameters.SYSTEM_SERVER_FEATURES |= (int16_t)common_types::EServerFeatures::RETRANSLATION;
        }
        else if( "archiving" == feature ){
            m_parameters.SYSTEM_SERVER_FEATURES |= (int16_t)common_types::EServerFeatures::ARCHIVING;
        }
        else if( "analyze" == feature ){
            m_parameters.SYSTEM_SERVER_FEATURES |= (int16_t)common_types::EServerFeatures::ANALYZE;
        }
        else{
            PRELOG_ERR << PRINT_HEADER << " unknown server feature [" << feature << "]" << endl;
            return false;
        }
    }


    boost::property_tree::ptree communication = config.get_child("communication");
    boost::property_tree::ptree httpServer = communication.get_child("http_server");
    m_parameters.COMMUNICATION_HTTP_SERVER_ENABLE = setParameterNew<bool>( httpServer, "enable", false );
    m_parameters.COMMUNICATION_HTTP_SERVER_PORT = setParameterNew<int32_t>( httpServer, "port", 5555 );
    boost::property_tree::ptree websocketServer = communication.get_child("websocket_server");
    m_parameters.COMMUNICATION_WEBSOCKET_SERVER_ENABLE = setParameterNew<bool>( websocketServer, "enable", false );
    m_parameters.COMMUNICATION_WEBSOCKET_SERVER_PORT = setParameterNew<int32_t>( websocketServer, "port", 5555 );
    boost::property_tree::ptree amqpClient = communication.get_child("amqp_client");
    m_parameters.COMMUNICATION_AMQP_ENABLE = setParameterNew<bool>( amqpClient, "enable", false );
    m_parameters.COMMUNICATION_AMQP_SERVER_HOST = setParameterNew<std::string>( amqpClient, "host", string("localhost") );
    m_parameters.COMMUNICATION_AMQP_VIRTUAL_HOST = setParameterNew<std::string>( amqpClient, "virt_host", string("safecity") );
    m_parameters.COMMUNICATION_AMQP_SERVER_PORT = setParameterNew<int32_t>( amqpClient, "port", 5672 );
    m_parameters.COMMUNICATION_AMQP_LOGIN = setParameterNew<std::string>( amqpClient, "login", string("scuser") );
    m_parameters.COMMUNICATION_AMQP_PASS = setParameterNew<std::string>( amqpClient, "pass", string("scpass") );
    boost::property_tree::ptree objreprBus = communication.get_child("objrepr_service_bus");
    m_parameters.COMMUNICATION_OBJREPR_SERVICE_BUS_ENABLE = setParameterNew<bool>( objreprBus, "enable", false );


    boost::property_tree::ptree objrepr = config.get_child("objrepr");
    m_parameters.OBJREPR_CONFIG_PATH = setParameterNew<std::string>( objrepr, "config_path", string("objrepr_cfg.xml") );
    m_parameters.OBJREPR_INITIAL_CONTEXT_NAME = setParameterNew<std::string>( objrepr, "initial_context_name", string("") );
    m_parameters.OBJREPR_GDM_VIDEO_CONTAINER_CLASSINFO_NAME = setParameterNew<std::string>( objrepr, "gdm_video_container_name", string("Система анализа видео") );


    boost::property_tree::ptree mongoDB = config.get_child("mongo_db");
    m_parameters.MONGO_DB_ADDRESS = setParameterNew<std::string>( mongoDB, "host", string("localhosst") );
    m_parameters.MONGO_DB_NAME = setParameterNew<std::string>( mongoDB, "database_name", string("video_server") );
    m_parameters.MONGO_LOGGER_TABLE = setParameterNew<std::string>( mongoDB, "logger_table", string("log") );    

    // parse derive part
    if( ! parse(_content) ){
        return false;
    }

    return true;
}

bool AConfigReader::isParametersCorrect(){

    // TODO: do

    return true;
}

std::string AConfigReader::getConfigFilePath(){

    // 1. from command line
    PRELOG_INFO << PRINT_HEADER << " try to find out config file in command line args" << endl;
    if( ! m_settings.mainConfigPath.empty() ){
        return m_settings.mainConfigPath;
    }

    // 2. from environment variable
    const string ENV_VAR_VIDEO_SERVER_CONFIG_PATH = boost::to_upper_copy<std::string>(m_settings.projectName) + "_MAIN_CONF_PATH";
    PRELOG_INFO << PRINT_HEADER << " try to find out config file in env var [" << ENV_VAR_VIDEO_SERVER_CONFIG_PATH << "]" << endl;
    const char * envariableValue = ::getenv( ENV_VAR_VIDEO_SERVER_CONFIG_PATH.c_str() );
    if( envariableValue ){
        const string envVarValue( envariableValue );
        if( ! envVarValue.empty() ){
            return envVarValue;
        }
    }

    // 3. from home dir
    const string path = string( ::getenv("HOME") ) + "/.config/" + m_settings.projectName + "/" + g_default_main_config_file_name;
    PRELOG_INFO << PRINT_HEADER << " try to find out config file in [" << path << "]" << endl;
    ifstream config( path, std::ios::in );
    if( config.is_open() ){
        config.close();
        return path;
    }

    // 4. from /etc dir
    const string path2 = string("/etc/") + m_settings.projectName + "/" + g_default_main_config_file_name;
    PRELOG_INFO << PRINT_HEADER << " try to find out config file in [" << path2 << "]" << endl;
    ifstream config2( path2, std::ios::in );
    if( config2.is_open() ){
        config2.close();
        return path2;
    }

    return "";
}


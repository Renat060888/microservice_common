#ifndef A_CONFIG_READER_H
#define A_CONFIG_READER_H

#include <string>
#include <map>

#include "communication/network_interface.h"
#include "communication/unified_command_convertor.h"

class AConfigReader
{
public:
    struct SConfigParameters {
        // TODO: valid values range & existing ( isParametersCorrect() )
        SConfigParameters()
            : SYSTEM_LOG_TO_STDOUT(false)
            , SYSTEM_LOG_TO_FILE(false)
            , SYSTEM_LOGFILE_ROTATION_SIZE_MB(0)
            , SYSTEM_CONNECT_RETRIES(0)
            , SYSTEM_CONNECT_RETRY_PERIOD_SEC(0)
            , SYSTEM_CATCH_CHILD_PROCESSES_OUTPUT(false)
            , SYSTEM_SELF_SHUTDOWN_SEC(0)

            , COMMUNICATION_HTTP_SERVER_ENABLE(false)
            , COMMUNICATION_WEBSOCKET_SERVER_ENABLE(false)
            , COMMUNICATION_HTTP_SERVER_PORT(0)
            , COMMUNICATION_WEBSOCKET_SERVER_PORT(0)
            , COMMUNICATION_AMQP_SERVER_PORT(0)
        {}

        std::vector<PEnvironmentRequest> INITIAL_REQUESTS;

        std::string SYSTEM_UNILOG_CONFIG_PATH;
        bool SYSTEM_LOG_TO_STDOUT;
        bool SYSTEM_LOG_TO_FILE;
        std::string SYSTEM_LOGFILE_NAME;
        std::string SYSTEM_REGULAR_LOGFILE_PATH;
        std::string SYSTEM_CRUSH_LOGFILE_PATH;
        int32_t SYSTEM_LOGFILE_ROTATION_SIZE_MB;
        int32_t SYSTEM_CONNECT_RETRIES;
        int32_t SYSTEM_CONNECT_RETRY_PERIOD_SEC;
        std::string SYSTEM_UNIQUE_ID;
        bool SYSTEM_CATCH_CHILD_PROCESSES_OUTPUT;
        int32_t SYSTEM_SELF_SHUTDOWN_SEC;
        int16_t SYSTEM_SERVER_FEATURES;
        bool SYSTEM_RESTORE_INTERRUPTED_SESSION;

        bool COMMUNICATION_HTTP_SERVER_ENABLE;
        int32_t COMMUNICATION_HTTP_SERVER_PORT;
        bool COMMUNICATION_WEBSOCKET_SERVER_ENABLE;
        int COMMUNICATION_WEBSOCKET_SERVER_PORT;
        bool COMMUNICATION_AMQP_ENABLE;
        std::string COMMUNICATION_AMQP_SERVER_HOST;
        std::string COMMUNICATION_AMQP_VIRTUAL_HOST;
        int COMMUNICATION_AMQP_SERVER_PORT;
        std::string COMMUNICATION_AMQP_LOGIN;
        std::string COMMUNICATION_AMQP_PASS;

        std::string OBJREPR_CONFIG_PATH;
        std::string OBJREPR_GDM_VIDEO_CONTAINER_CLASSINFO_NAME;
        std::string OBJREPR_INITIAL_CONTEXT_NAME;

        std::string MONGO_DB_ADDRESS;
        std::string MONGO_DB_NAME;
        std::string MONGO_LOGGER_TABLE;        
    };

    struct SIninSettings {
        SIninSettings()
            : commandConvertor(nullptr)
            , env(nullptr)
        {}
        UnifiedCommandConvertor * commandConvertor;
        std::string mainConfigPath;
        char ** env;
        std::string projectName;
    };

    bool init( const SIninSettings & _settings );
    const SConfigParameters & get(){ return m_parameters; }

    void printToStdoutConfigExample();


protected:
    AConfigReader();
    virtual ~AConfigReader();

    AConfigReader( const AConfigReader & _inst ) = delete;
    AConfigReader & operator=( const AConfigReader & _inst ) = delete;

    virtual bool initDerive( const SIninSettings & _settings ) = 0;
    virtual bool parse( const std::string & _filePath ) = 0;
    virtual bool createCommandsFromConfig( const std::string & _content ) = 0;
    virtual void printToStdoutConfigExampleDerive() = 0;


private:
    bool parseBase( const std::string & _filePath );
    bool isParametersCorrect();

    std::string getConfigFilePath();
    bool createResourcesDir( const std::string & _rootDir, const std::string & _serverName );


    // data
    SIninSettings m_settings;
    SConfigParameters m_parameters;

    // service
    UnifiedCommandConvertor * m_commandConvertor;



};

#endif // A_CONFIG_READER_H

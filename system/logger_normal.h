#ifdef SWITCH_LOGGER_DEBIAN_9

#ifndef LOGGER_H
#define LOGGER_H

#include "spdlog/spdlog.h"

#include "logger_common.h"

enum class ELogEndpoints {
    Stdout      = 1 << 0,
    File        = 1 << 1,
    MongoDB     = 1 << 2,
    Network     = 1 << 3,
    FuncPtr     = 1 << 4,
    Undefined   = 1 << 31
};

using TFuncPtr = void( * )( const std::string & _msg );

class Logger final {

public:
    struct SInitSettings {
        SInitSettings() :
            logEndpoints(ELogEndpoints::Stdout)
        {}
        std::string loggerName;
        ELogEndpoints logEndpoints;
        // optional
        TFuncPtr funcPtr;
        std::string fileName;
        std::string filePath;
        int32_t rotationSizeMb;
    };

    static Logger & singleton(){
        static Logger instance;
        return instance;
    }

    void initGlobal( const SInitSettings & _settings );

    void setLevel( spdlog::level::level_enum _level );
    void setFuncPtr( TFuncPtr _funcPtr );
    void setFileName( const std::string & _fileName, int32_t _rotationSizeMb );

    std::shared_ptr<spdlog::logger> m_logRegular;

private:
    Logger();
    ~Logger();

    Logger & operator=( const Logger & ) = delete;
    Logger( const Logger & ) = delete;

    std::vector<spdlog::sink_ptr> m_sinks;
    spdlog::level::level_enum m_level;


};

#define LOG_INFO Logger::singleton().m_logRegular->info
#define LOG_WARN Logger::singleton().m_logRegular->warn
#define LOG_DBG Logger::singleton().m_logRegular->debug
#define LOG_ERROR Logger::singleton().m_logRegular->error
#define LOG_CRITICAL Logger::singleton().m_logRegular->critical

#endif // LOGGER_H

#endif // DEBIAN_9

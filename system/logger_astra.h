#ifdef SWITCH_LOGGER_ASTRA

#ifndef LOGGER_ASTRA_H
#define LOGGER_ASTRA_H

#include <unilog/unilog.h>

#include "logger_common.h"

class Logger final {
public:
    static Logger & singleton(){
        static Logger instance;
        return instance;
    }

    void initGlobal( const logger_common::SInitSettings & _settings );

    unilog::LoggerPtr _lg;

private:
    Logger();
    ~Logger();

    Logger & operator=( const Logger & ) = delete;
    Logger( const Logger & ) = delete;
};

#define VS_LOG_INFO UNILOG_INFO(Logger::singleton()._lg)
#define VS_LOG_WARN UNILOG_WARNING(Logger::singleton()._lg)
#define VS_LOG_DBG UNILOG_DEBUG(Logger::singleton()._lg)
#define VS_LOG_TRACE UNILOG_TRACE(Logger::singleton()._lg)
#define VS_LOG_ERROR UNILOG_ERROR(Logger::singleton()._lg)
#define VS_LOG_CRITICAL UNILOG_CRIT(Logger::singleton()._lg)

#endif // LOGGER_ASTRA_H

#endif // CURRENT_VERSION

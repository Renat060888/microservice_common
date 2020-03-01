#ifndef LOGGER_SIMPLE_H
#define LOGGER_SIMPLE_H

#include <iostream>

#include "logger_common.h"

class Logger final
{
public:       
    static Logger & singleton(){
        static Logger instance;
        return instance;
    }

    void initGlobal( const logger_common::SInitSettings & ){ /*dummy*/ }


private:
    Logger();
    ~Logger();

    Logger & operator=( const Logger & ) = delete;
    Logger( const Logger & ) = delete;


};

#define VS_LOG_INFO std::cout
#define VS_LOG_DBG std::cout
#define VS_LOG_TRACE std::cout
#define VS_LOG_WARN std::cerr
#define VS_LOG_ERROR std::cerr
#define VS_LOG_CRITICAL std::cerr

#endif // LOGGER_SIMPLE_H

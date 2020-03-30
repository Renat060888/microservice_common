#ifdef SWITCH_LOGGER_DEBIAN_9

// 3rd party
#include "spdlog/sinks/ansicolor_sink.h"
#include "spdlog/sinks/file_sinks.h"
// project
#include "logger.h"
#include "spdlog_sink_mongodb.h"
#include "spdlog_sink_func.h"

using namespace std;

Logger::Logger() :
    m_level(spdlog::level::debug)
{

}

Logger::~Logger(){

}

void Logger::initGlobal( const SInitSettings & _settings ){

    m_sinks.clear();

    if( (int)_settings.logEndpoints & (int)ELogEndpoints::Stdout ){

//        auto sinkStdout = make_shared<spdlog::sinks::ansicolor_sink>();
//        auto sinkStdout = spdlog::sinks::stdout_sink_mt::instance();
//        auto sinkStdout = make_shared<spdlog::sinks::stdout_color_sink_mt>(); // 1.3 version
        auto sinkStdout = make_shared<spdlog::sinks::stdout_sink_mt>(); // 0.11 version
        m_sinks.push_back( sinkStdout );
    }
    if( (int)_settings.logEndpoints & (int)ELogEndpoints::File ){

        assert( ! _settings.fileName.empty() );
        int maxRotationFiles = 1;
        auto sinkFile = make_shared<spdlog::sinks::rotating_file_sink_mt>(  _settings.filePath + "/" + _settings.fileName + "_regular",
                                                                            "log",
                                                                            _settings.rotationSizeMb * 1024 * 1024,
                                                                            maxRotationFiles );
        m_sinks.push_back( sinkFile );
    }
    if( (int)_settings.logEndpoints & (int)ELogEndpoints::MongoDB ){


        auto sinkMongoDB = make_shared<SpdlogSinkMongoDB>();
        m_sinks.push_back( sinkMongoDB );
    }
    if( (int)_settings.logEndpoints & (int)ELogEndpoints::FuncPtr ){

//        assert( _settings.funcPtr );
//        auto sinkMongoDB = make_shared<SpdlogSinkFunc>( _settings.funcPtr );
//        m_sinks.push_back( sinkMongoDB );
    }
    if( (int)_settings.logEndpoints & (int)ELogEndpoints::Network ){
        // TODO: do
    }

    m_logRegular = make_shared<spdlog::logger>( _settings.loggerName, std::begin(m_sinks), std::end(m_sinks) );
//    std::shared_ptr<spdlog::logger> log = make_shared<spdlog::logger>( _loggerName, std::begin(m_sinks), std::end(m_sinks) );

    m_logRegular->set_level( m_level );
    m_logRegular->flush_on( m_level );
}

void Logger::setLevel( spdlog::level::level_enum _level ){

    m_level = _level;
    m_logRegular->set_level( m_level );
}

#endif // DEBIAN_9

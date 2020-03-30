#ifdef SWITCH_LOGGER_ASTRA

#include "logger_astra.h"
#include "../common/ms_common_utils.h"

using namespace std;
using namespace logger_common;

static constexpr const char * PRINT_HEADER = "Logger:";

Logger::Logger()
{

}

Logger::~Logger(){

}

void Logger::initGlobal( const SInitSettings & _settings ){

    if( (int)_settings.logEndpoints & (int)ELogEndpoints::Stdout ){

    }
    if( (int)_settings.logEndpoints & (int)ELogEndpoints::File ){
        assert( ! _settings.fileName.empty() );
    }
    if( (int)_settings.logEndpoints & (int)ELogEndpoints::MongoDB ){
        assert( false );
    }
    if( (int)_settings.logEndpoints & (int)ELogEndpoints::FuncPtr ){
        assert( false );
    }
    if( (int)_settings.logEndpoints & (int)ELogEndpoints::Network ){
        assert( false );
    }

    if( ! unilog::Unilog::instance()->configure( _settings.unilogConfigPath.c_str() ) ){
        PRELOG_ERR << PRINT_HEADER << " ERROR - unilog configure failed [" << unilog::Unilog::instance()->errString() << "]" << endl;
    }
    _lg = unilog::Unilog::instance()->getLogger( unilog::Unilog::FC_OTHER, _settings.loggerName.c_str() );
}

#endif // ASTRA_15

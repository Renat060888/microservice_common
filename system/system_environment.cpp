
#include <iostream>
#include <sys/stat.h>

#include <microservice_common/common/ms_common_utils.h>
#include <microservice_common/system/logger.h>
#include <microservice_common/system/process_launcher.h>
#include <microservice_common/system/objrepr_bus.h>

#include "system_environment.h"
#include "config_reader.h"
#include "args_parser.h"
#include "path_locator.h"

using namespace std;

static constexpr const char * PRINT_HEADER = "SystemEnvironment:";

SystemEnvironment::SystemEnvironment()
    : m_database(nullptr)
{

}

SystemEnvironment::~SystemEnvironment()
{
    VS_LOG_INFO << PRINT_HEADER << " begin shutdown" << endl;

    DatabaseManager::destroyInstance( m_database );

    OBJREPR_BUS.shutdown();

    VS_LOG_INFO << PRINT_HEADER << " shutdown success" << endl;
}

bool SystemEnvironment::init( SInitSettings _settings ){

    m_state.settings = _settings;

    if( ! isApplicationInstanceUnique() ){
        return false;
    }

    // TODO: what for ?
    ::umask( 0 );

    if( ! OBJREPR_BUS.init() ){
        VS_LOG_CRITICAL << "objrepr init failed: " << OBJREPR_BUS.getLastError() << endl;
        return false;
    }    

    DatabaseManager::SInitSettings settings3;
    settings3.host = CONFIG_PARAMS.MONGO_DB_ADDRESS;
    settings3.databaseName = CONFIG_PARAMS.MONGO_DB_NAME;

    m_database = DatabaseManager::getInstance();
    if( ! m_database->init(settings3) ){
        return false;
    }

    WriteAheadLogger::SInitSettings settings2;
    settings2.active = CONFIG_PARAMS.SYSTEM_RESTORE_INTERRUPTED_SESSION;
    settings2.persistService = this;

    if( ! m_wal.init(settings2) ){
        return false;
    }

    if( ! CONFIG_PARAMS.SYSTEM_RESTORE_INTERRUPTED_SESSION ){
        m_wal.cleanJournal();
    }

    const std::vector<common_types::TPid> zombieChildProcesses = m_wal.getNonClosedProcesses();
    for( const common_types::TPid pid : zombieChildProcesses ){
        PROCESS_LAUNCHER.kill( pid );
    }

    writePidFile();

    VS_LOG_INFO << PRINT_HEADER << " init success" << endl;
    return true;
}

WriteAheadLogger * SystemEnvironment::serviceForWriteAheadLogging(){

    return & m_wal;
}

bool SystemEnvironment::isApplicationInstanceUnique(){

    const int pidFile = ::open( PATH_LOCATOR.getUniqueLockFile().c_str(), O_CREAT | O_RDWR | O_TRUNC, 0666 );

    const int rc = ::flock( pidFile, LOCK_EX | LOCK_NB );
    if( rc ){
        if( EWOULDBLOCK == errno ){
            VS_LOG_ERROR << "CRITICAL: another instance of Video Server already is running."
                      << " (file already locked: " << PATH_LOCATOR.getUniqueLockFile() << ")"
                      << " Abort"
                      << endl;
            return false;
        }
    }

    return true;
}

void SystemEnvironment::writePidFile(){

    const char * pidStr = std::to_string( ::getpid() ).c_str();
    VS_LOG_INFO << "write pid [" << pidStr << "]"
             << " to pid file [" << PATH_LOCATOR.getUniqueLockFile() << "]"
             << endl;

    ofstream pidFile( PATH_LOCATOR.getUniqueLockFile(), std::ios::out | std::ios::trunc );
    if( ! pidFile.is_open() ){
        VS_LOG_ERROR << "cannot open pid file for write" << endl;
    }

    pidFile << pidStr;
}

bool SystemEnvironment::openContext( common_types::TContextId _ctxId ){

    if( ! OBJREPR_BUS.openContextAsync(_ctxId) ){
        m_state.lastError = OBJREPR_BUS.getLastError();
        return false;
    }
    return true;


}

bool SystemEnvironment::closeContext(){

    if( ! OBJREPR_BUS.closeContext() ){
        m_state.lastError = OBJREPR_BUS.getLastError();
        return false;
    }
    return true;

}

bool SystemEnvironment::write( const common_types::SWALClientOperation & _clientOperation ){

    return m_database->writeClientOperation( _clientOperation );
}

void SystemEnvironment::remove( common_types::SWALClientOperation::TUniqueKey _filter ){

    m_database->removeClientOperation( _filter );
}

const std::vector<common_types::SWALClientOperation> SystemEnvironment::readOperations( common_types::SWALClientOperation::TUniqueKey _filter ){

    if( common_types::SWALClientOperation::ALL_KEYS == _filter ){
        return m_database->getClientOperations();
    }
    else if( common_types::SWALClientOperation::NON_INTEGRITY_KEYS == _filter ){
        return m_database->getNonIntegrityClientOperations();
    }
    else{
        assert( false );
    }
}

bool SystemEnvironment::write( const common_types::SWALProcessEvent & _processEvent ){

    return m_database->writeProcessEvent( _processEvent, true );
}

void SystemEnvironment::remove( common_types::SWALProcessEvent::TUniqueKey _filter ){

    m_database->removeProcessEvent( _filter );
}

const std::vector<common_types::SWALProcessEvent> SystemEnvironment::readEvents( common_types::SWALProcessEvent::TUniqueKey _filter ){

    if( common_types::SWALProcessEvent::ALL_PIDS != _filter && common_types::SWALProcessEvent::NON_INTEGRITY_PIDS != _filter ){
        return m_database->getProcessEvents( _filter );
    }
    else if( common_types::SWALProcessEvent::ALL_PIDS == _filter ){
        return m_database->getProcessEvents();
    }
    else if( common_types::SWALProcessEvent::NON_INTEGRITY_PIDS == _filter ){
        return m_database->getNonIntegrityProcessEvents();
    }
    else{
        assert( false );
    }
}






#include <iostream>
#include <sys/stat.h>

#include "common/ms_common_utils.h"
#include "logger.h"
#include "process_launcher.h"
#include "objrepr_bus.h"
#include "system_environment_facade.h"

using namespace std;
using namespace common_types;

static constexpr const char * PRINT_HEADER = "SystemEnvironment:";

SystemEnvironmentFacade::SystemEnvironmentFacade()
    : m_database(nullptr)
{

}

SystemEnvironmentFacade::~SystemEnvironmentFacade()
{
    VS_LOG_INFO << PRINT_HEADER << " begin shutdown" << endl;

    DatabaseManagerBase::destroyInstance( m_database );

    OBJREPR_BUS.shutdown();

    VS_LOG_INFO << PRINT_HEADER << " shutdown success" << endl;
}

bool SystemEnvironmentFacade::init( SInitSettings _settings ){

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

    DatabaseManagerBase::SInitSettings settings3;
    settings3.host = _settings.databaseHost;
    settings3.databaseName = _settings.databaseName;

    m_database = DatabaseManagerBase::getInstance();
    if( ! m_database->init(settings3) ){
        return false;
    }

    WriteAheadLogger::SInitSettings settings2;
    settings2.active = _settings.restoreSystemAfterInterrupt;
    settings2.persistService = this;

    if( ! m_wal.init(settings2) ){
        return false;
    }

    if( ! _settings.restoreSystemAfterInterrupt ){
        m_wal.cleanJournal();
    }

    const std::vector<common_types::TPid> zombieChildProcesses = m_wal.getNonClosedProcesses();
    for( const common_types::TPid pid : zombieChildProcesses ){
        PROCESS_LAUNCHER.kill( pid );
    }

    writePidFile();

    if( ! initDerive() ){
        return false;
    }

    VS_LOG_INFO << PRINT_HEADER << " init success" << endl;
    return true;
}

WriteAheadLogger * SystemEnvironmentFacade::serviceForWriteAheadLogging(){

    return & m_wal;
}

bool SystemEnvironmentFacade::isApplicationInstanceUnique(){

    const int pidFile = ::open( m_state.settings.uniqueLockFileFullPath.c_str(), O_CREAT | O_RDWR | O_TRUNC, 0666 );

    const int rc = ::flock( pidFile, LOCK_EX | LOCK_NB );
    if( rc ){
        if( EWOULDBLOCK == errno ){
            VS_LOG_ERROR << "CRITICAL: another instance of Video Server already is running."
                      << " (file already locked: " << m_state.settings.uniqueLockFileFullPath << ")"
                      << " Abort"
                      << endl;
            return false;
        }
    }

    return true;
}

void SystemEnvironmentFacade::writePidFile(){

    const char * pidStr = std::to_string( ::getpid() ).c_str();
    VS_LOG_INFO << "write pid [" << pidStr << "]"
             << " to pid file [" << m_state.settings.uniqueLockFileFullPath << "]"
             << endl;

    ofstream pidFile( m_state.settings.uniqueLockFileFullPath, std::ios::out | std::ios::trunc );
    if( ! pidFile.is_open() ){
        VS_LOG_ERROR << "cannot open pid file for write" << endl;
    }

    pidFile << pidStr;
}

bool SystemEnvironmentFacade::openContext( common_types::TContextId _ctxId ){

    if( ! OBJREPR_BUS.openContextAsync(_ctxId) ){
        m_state.lastError = OBJREPR_BUS.getLastError();
        return false;
    }
    return true;


}

bool SystemEnvironmentFacade::closeContext(){

    if( ! OBJREPR_BUS.closeContext() ){
        m_state.lastError = OBJREPR_BUS.getLastError();
        return false;
    }
    return true;

}

bool SystemEnvironmentFacade::write( const common_types::SWALClientOperation & _clientOperation ){

    return m_database->writeClientOperation( _clientOperation );
}

void SystemEnvironmentFacade::remove( common_types::SWALClientOperation::TUniqueKey _filter ){

    m_database->removeClientOperation( _filter );
}

const std::vector<common_types::SWALClientOperation> SystemEnvironmentFacade::readOperations( common_types::SWALClientOperation::TUniqueKey _filter ){

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

bool SystemEnvironmentFacade::write( const SWALUserRegistration & _userRegistration ){

    return m_database->writeUserRegistration( _userRegistration );
}

void SystemEnvironmentFacade::removeRegistration( SWALUserRegistration::TRegisterId _filter ){

    m_database->removeUserRegistration( _filter );
}

const std::vector<SWALUserRegistration> SystemEnvironmentFacade::readRegistrations( SWALUserRegistration::TRegisterId _filter ){

    return m_database->getUserRegistrations();
}

bool SystemEnvironmentFacade::write( const common_types::SWALProcessEvent & _processEvent ){

    return m_database->writeProcessEvent( _processEvent, true );
}

void SystemEnvironmentFacade::remove( common_types::SWALProcessEvent::TUniqueKey _filter ){

    m_database->removeProcessEvent( _filter );
}

const std::vector<common_types::SWALProcessEvent> SystemEnvironmentFacade::readEvents( common_types::SWALProcessEvent::TUniqueKey _filter ){

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





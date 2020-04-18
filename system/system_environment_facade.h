#ifndef SYSTEM_ENVIRONMENT_FACADE_H
#define SYSTEM_ENVIRONMENT_FACADE_H

#include <unordered_map>

#include "wal.h"
#include "storage/database_manager_base.h"
#include "common/ms_common_types.h"

class SystemEnvironmentFacade : public common_types::IWALPersistenceService
{
public:
    struct SServiceLocator {
        SServiceLocator()
        {}        
        common_types::IContextService * contextControlService;
    };

    struct SInitSettings {
        SInitSettings()
        {}

        // database
        std::string databaseHost;
        std::string databaseName;
        // wal
        bool restoreSystemAfterInterrupt;
        // file stuff
        std::string uniqueLockFileFullPath;

        SServiceLocator services;
    };

    struct SState {
        SInitSettings settings;
        std::string lastError;
    };

    SystemEnvironmentFacade();
    ~SystemEnvironmentFacade();

    bool init( const SInitSettings & _settings );
    const SState & getState(){ return m_state; }

    WriteAheadLogger * serviceForWriteAheadLogging();
    common_types::IContextService * serviceForContextControl();

    // NOTE: custom services will be in derived classes


    // TODO: move here system monitor & path locator ?


protected:
    virtual bool initDerive(){ return true; }


private:
    bool isApplicationInstanceUnique( const std::string & _lockFileFullPath );
    void writePidFile( const std::string & _pidFileFullPath );

    // wal persistence functional is a part of 'SystemEnvironment' so far
    virtual bool write( const common_types::SWALClientOperation & _clientOperation ) override;
    virtual void remove( common_types::SWALClientOperation::TUniqueKey _filter ) override;
    virtual const std::vector<common_types::SWALClientOperation> readOperations( common_types::SWALClientOperation::TUniqueKey _filter ) override;

    virtual bool write( const common_types::SWALProcessEvent & _processEvent ) override;
    virtual void remove( common_types::SWALProcessEvent::TUniqueKey _filter ) override;
    virtual const std::vector<common_types::SWALProcessEvent> readEvents( common_types::SWALProcessEvent::TUniqueKey _filter ) override;

    virtual bool write( const common_types::SWALUserRegistration & _userRegistration ) override;
    virtual void removeRegistration( common_types::SWALUserRegistration::TRegisterId _filter ) override;
    virtual const std::vector<common_types::SWALUserRegistration> readRegistrations( common_types::SWALUserRegistration::TRegisterId _filter ) override;

    // data
    SState m_state;

    // service
    WriteAheadLogger m_wal;
    DatabaseManagerBase * m_database;
};

#endif // SYSTEM_ENVIRONMENT_FACADE_H


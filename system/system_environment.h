#ifndef SYSTEM_ENVIRONMENT_H
#define SYSTEM_ENVIRONMENT_H

#include <unordered_map>

#include <microservice_common/system/wal.h>
#include <microservice_common/storage/database_manager_base.h>

#include "common/common_types.h"

class SystemEnvironment : public common_types::IWALPersistenceService
{
public:
    struct SServiceLocator {
        SServiceLocator()
        {}        
    };

    struct SInitSettings {
        SInitSettings()
        {}
        SServiceLocator services;
    };

    struct SState {
        SInitSettings settings;
        std::string lastError;
    };

    SystemEnvironment();
    ~SystemEnvironment();

    bool init( SInitSettings _settings );
    const SState & getState(){ return m_state; }

    bool openContext( common_types::TContextId _ctxId );
    bool closeContext();

    WriteAheadLogger * serviceForWriteAheadLogging();

private:
    bool isApplicationInstanceUnique();
    void writePidFile();

    // wal persistence functional is a part of 'SystemEnvironment' so far
    virtual bool write( const common_types::SWALClientOperation & _clientOperation ) override;
    virtual void remove( common_types::SWALClientOperation::TUniqueKey _filter ) override;
    virtual const std::vector<common_types::SWALClientOperation> readOperations( common_types::SWALClientOperation::TUniqueKey _filter ) override;

    virtual bool write( const common_types::SWALProcessEvent & _processEvent ) override;
    virtual void remove( common_types::SWALProcessEvent::TUniqueKey _filter ) override;
    virtual const std::vector<common_types::SWALProcessEvent> readEvents( common_types::SWALProcessEvent::TUniqueKey _filter ) override;

    // data
    SState m_state;

    // service
    WriteAheadLogger m_wal;
    DatabaseManager * m_database;




};

#endif // SYSTEM_ENVIRONMENT_H


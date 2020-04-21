#ifndef WAL_H
#define WAL_H

#include "common/ms_common_types.h"
#include "communication/network_interface.h"

// component for 1. system reliability and 2. play the role of a journal
class WriteAheadLogger
{
public:
    struct SInitSettings {
        SInitSettings()
            : active(false)
            , persistService(nullptr)
        {}
        bool active;
        common_types::IWALPersistenceService * persistService;
    };

    WriteAheadLogger();
    ~WriteAheadLogger();

    bool init( const SInitSettings & _settings );
    std::string getFullHistory();
    void cleanJournal();

    // client operation
    void openOperation( const common_types::SWALClientOperation & _operation );
    void closeClientOperation( common_types::SWALClientOperation::TUniqueKey _uniqueKey );
    std::vector<PEnvironmentRequest> getInterruptedOperations();

    // child process
    void openProcessEvent( const common_types::SWALProcessEvent & _event );
    void closeProcessEvent( common_types::SWALProcessEvent::TUniqueKey _pid );
    std::vector<common_types::TPid> getNonClosedProcesses();

    // user registration
    bool openUserRegistration( const common_types::SWALUserRegistration & _registration );
    void closeUserRegistration( common_types::SWALUserRegistration::TRegisterId _id );
    std::vector<common_types::SWALUserRegistration> getUserRegistrations();

private:
    bool isJournalEmpty();


    // data
    SInitSettings m_settings;

    // service


};

#endif // WAL_H

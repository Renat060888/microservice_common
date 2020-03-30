
#include <sstream>

#include "logger.h"
#include "wal.h"

using namespace std;

static constexpr const char * PRINT_HEADER = "WAL:";

// - client operations
// +----+-----------+----------+------------+-----------+
// | id | begin/end | cmd type | unique key | full text |
// +----+-----------+----------+------------+-----------+
// |    |           |          |            |           |
// |    |           |                       |           |
// |    |                                   |           |
// |                                        |
//                                          |
//                                          |

// - child processes
// +----+-------------+-----+------+------+
// | id | launch/stop | pid | name | args |
// +----+-------------+-----+------+------+
// |    |             |     |      |      |
// |    |             |            |      |
// |    |                          |      |
// |                               |
//                                 |
//

// -----------------------------------------------------------------------------
// request override
// -----------------------------------------------------------------------------
class RequestFromWAL : public AEnvironmentRequest {

public:
    virtual void setOutcomingMessage( const std::string & /*_msg*/ ) override {
        // dummy
    }
};
using PRequestFromWAL = std::shared_ptr<RequestFromWAL>;

//
WriteAheadLogger::WriteAheadLogger()
{

}

WriteAheadLogger::~WriteAheadLogger()
{

}

bool WriteAheadLogger::init( const SInitSettings & _settings ){

    m_settings = _settings;



    return true;
}

void WriteAheadLogger::openOperation( const common_types::SWALClientOperation & _operation ){

    if( ! m_settings.active ){
        return;
    }

    if( _operation.begin ){
        const bool rt = m_settings.persistService->write( _operation );
        if( ! rt ){
            // TODO: do
        }
    }
    else{
        m_settings.persistService->remove( _operation.uniqueKey );
    }
}

void WriteAheadLogger::closeClientOperation( common_types::SWALClientOperation::TUniqueKey _uniqueKey ){

    m_settings.persistService->remove( _uniqueKey );
}

std::vector<PEnvironmentRequest> WriteAheadLogger::getInterruptedOperations(){

    if( ! m_settings.active ){
        return std::vector<PEnvironmentRequest>();
    }

    // NOTE: or do manual by C++
    const std::vector<common_types::SWALClientOperation> operations = m_settings.persistService->readOperations( common_types::SWALClientOperation::NON_INTEGRITY_KEYS );

    //
    std::vector<PEnvironmentRequest> out;

    for( const common_types::SWALClientOperation & oper : operations ){

        VS_LOG_INFO << PRINT_HEADER << " found interruped client operation [" << oper.commandFullText << "]" << endl;

        PRequestFromWAL request = std::make_shared<RequestFromWAL>();
        request->m_incomingMessage = oper.commandFullText;

        out.push_back( request );
    }

    return out;
}

void WriteAheadLogger::openProcessEvent( const common_types::SWALProcessEvent & _event ){

    if( ! m_settings.active ){
        return;
    }

    const bool rt = m_settings.persistService->write( _event );
    if( ! rt ){
        // TODO: do
    }
}

void WriteAheadLogger::closeProcessEvent( common_types::SWALProcessEvent::TUniqueKey _pid ){

    m_settings.persistService->remove( _pid );
}

std::vector<common_types::TPid> WriteAheadLogger::getNonClosedProcesses(){

    if( ! m_settings.active ){
        return std::vector<common_types::TPid>();
    }

    // NOTE: or do manual by C++
    const std::vector<common_types::SWALProcessEvent> events = m_settings.persistService->readEvents( common_types::SWALProcessEvent::NON_INTEGRITY_PIDS );

    //
    std::vector<common_types::TPid> out;

    for( const common_types::SWALProcessEvent & event : events ){

        VS_LOG_INFO << PRINT_HEADER << " found non closed process [" << event.pid << ", " << event.programName << "]" << endl;

        out.push_back( event.pid );
    }

    return out;
}

bool WriteAheadLogger::openUserRegistration( const common_types::SWALUserRegistration & _registration ){

    if( ! m_settings.active ){
        return true;
    }

    const bool rt = m_settings.persistService->write( _registration );
    if( ! rt ){
        // TODO: do
    }

    return true;
}

void WriteAheadLogger::closeUserRegistration( common_types::SWALUserRegistration::TRegisterId _id ){

    m_settings.persistService->removeRegistration( _id );
}

std::vector<common_types::SWALUserRegistration> WriteAheadLogger::getUserRegistrations(){

    if( ! m_settings.active ){
        return std::vector<common_types::SWALUserRegistration>();
    }

    // NOTE: or do manual by C++
    const std::vector<common_types::SWALUserRegistration> registrations = m_settings.persistService->readRegistrations( "" );
    return registrations;
}

string WriteAheadLogger::getFullHistory(){

    const std::vector<common_types::SWALProcessEvent> events = m_settings.persistService->readEvents( common_types::SWALProcessEvent::ALL_PIDS );
    const std::vector<common_types::SWALClientOperation> operations = m_settings.persistService->readOperations( common_types::SWALClientOperation::ALL_KEYS );

    if( events.empty() && operations.empty() ){
        stringstream ss;
        ss << endl << PRINT_HEADER << "======================== WAL History ========================" << endl;
        ss << "history is empty";
        ss << endl << PRINT_HEADER << "======================== WAL History ========================" << endl;
        return ss.str();
    }

    stringstream ss;
    ss << endl << PRINT_HEADER << "======================== WAL History ========================" << endl;

    ss << "process events: " << endl;
    if( events.empty() ){
        ss << "no events";
    }

    for( const common_types::SWALProcessEvent & event : events ){
        ss << " - " << event.serializeToStr();
    }

    ss << endl << "client operations:" << endl;
    if( operations.empty() ){
        ss << "no operations";
    }

    for( const common_types::SWALClientOperation & operation : operations ){
        ss << " - " << operation.serializeToStr();
    }
    ss << endl << PRINT_HEADER << "======================== WAL History ========================" << endl;

    return ss.str();
}

bool WriteAheadLogger::isJournalEmpty(){

    return ( m_settings.persistService->readEvents(common_types::SWALProcessEvent::ALL_PIDS).empty()
             && m_settings.persistService->readOperations( common_types::SWALClientOperation::ALL_KEYS ).empty() );
}

void WriteAheadLogger::cleanJournal(){

    VS_LOG_INFO << getFullHistory() << endl;

    if( ! isJournalEmpty() ){
        VS_LOG_INFO << PRINT_HEADER << " clean entire journal..." << endl;
        m_settings.persistService->remove( common_types::SWALClientOperation::ALL_KEYS );
        m_settings.persistService->remove( common_types::SWALProcessEvent::ALL_PIDS );
    }
}


















#include "threaded_multitask_service.h"

namespace threaded_multitask_service {

using namespace std;

ThreadedMultitaskService::ThreadedMultitaskService()
    : m_shutdownCalled(false)
    , m_threadDumpToDatabase(nullptr)
    , m_clientIdGenerator(0)
    , m_runnableTaskDumpToDatabase("<DUMP TO DATABASE>")
    , m_runnableTaskNetworkCallbacks("<NETWORK CALLBACKS>")
    , m_runnableTaskStateMonitoring("<STATE MONITORING>")
    , m_runnableTaskAsyncNotify("<ASYNC NOTIFY>")
{

}

ThreadedMultitaskService::~ThreadedMultitaskService()
{
    shutdown();
}

bool ThreadedMultitaskService::init(){



    m_threadDumpToDatabase = new std::thread( & ThreadedMultitaskService::threadDumpToDatabase, this );

    VS_LOG_INFO << PRINT_HEADER
             << " init success"
             << endl;

    return true;
}

void ThreadedMultitaskService::shutdown(){

    if( ! m_shutdownCalled.load() ){

        VS_LOG_INFO << PRINT_HEADER
                 << " initiate shutdown..."
                 << endl;

        m_shutdownCalled = true;

        common_utils::threadShutdown( m_threadDumpToDatabase );


        VS_LOG_INFO << PRINT_HEADER
                 << " ...success shutdown"
                 << endl;
    }
}

TRunnableClientId ThreadedMultitaskService::addRunnableClient( IRunnableDumpToDatabase * _client ){

    if( ! _client ){
        VS_LOG_ERROR << PRINT_HEADER
                  << " client for dump to database thread is NULL"
                  << endl;
        return INVALID_CLIENT_ID;
    }

    m_muDumpToDatabaseLock.lock();
    const TRunnableClientId clientId = ++m_clientIdGenerator;
    m_clientsForDumpToDatabase.insert( {clientId, _client} );
    m_muDumpToDatabaseLock.unlock();

    return clientId;
}

bool ThreadedMultitaskService::removeDumpToDatabaseClient( const TRunnableClientId _id ){

    if( _id <= INVALID_CLIENT_ID ){
        VS_LOG_ERROR << PRINT_HEADER
                  << " invalid dump to database client id"
                  << endl;
        return false;
    }

    m_muDumpToDatabaseLock.lock();
    const unordered_map<TRunnableClientId, IRunnableDumpToDatabase *>::size_type elementsErased = m_clientsForDumpToDatabase.erase( _id );
    m_muDumpToDatabaseLock.unlock();

    if( 0 == elementsErased ){
        VS_LOG_ERROR << PRINT_HEADER
                  << " such dump to database client id not found [" << _id << "]"
                  << endl;
        return false;
    }

    return true;
}

void ThreadedMultitaskService::threadDumpToDatabase(){

    VS_LOG_INFO << PRINT_HEADER
             << " dump to database thread is STARTED"
             << endl;

    while( ! m_shutdownCalled.load() ){

        VS_LOG_INFO << PRINT_HEADER
                 << " dump to database thread go to SLEEP"
                 << endl;

        std::mutex muCvLock;
        std::unique_lock<std::mutex> lock( muCvLock );
        m_cvDumpToDatabaseEvent.wait( lock, [this]{
            m_muDumpToDatabaseLock.lock();
            bool isClientExists = ! m_clientsForDumpToDatabase.empty();
            m_muDumpToDatabaseLock.unlock();
            return ( isClientExists || m_shutdownCalled.load() );
        } );

        VS_LOG_INFO << PRINT_HEADER
                 << " dump to database thread is WAKED UP"
                 << endl;

        if( m_shutdownCalled.load() ){
            break;
        }

        m_muDumpToDatabaseLock.lock();
        bool isClientExists = ! m_clientsForDumpToDatabase.empty();
        m_muDumpToDatabaseLock.unlock();

        while( isClientExists ){

            m_muDumpToDatabaseLock.lock();
            for( auto & valuePair : m_clientsForDumpToDatabase ){
                valuePair.second->runInThreadService();
            }

            isClientExists = ! m_clientsForDumpToDatabase.empty();
            m_muDumpToDatabaseLock.unlock();
        }
    }

    VS_LOG_INFO << PRINT_HEADER
             << " dump to database thread go to EXIT"
             << endl;
}

}







#ifndef THREADED_MULTITASK_SERVICE_H
#define THREADED_MULTITASK_SERVICE_H

#include <thread>
#include <atomic>
#include <condition_variable>
#include <unordered_map>

#include "logger.h"
#include "common/ms_common_utils.h"

namespace threaded_multitask_service {

using TRunnableClientId = int32_t;

static const std::string PRINT_HEADER = "<MULTITASK SERVICE>";
static const TRunnableClientId INVALID_CLIENT_ID = -1;

// -------------------------------------------------
// various tasks for threaded execution
// -------------------------------------------------
class IRunnableStateMonitoring {
public:
    virtual ~IRunnableStateMonitoring(){}
    virtual void runInThreadService() = 0;

};

class IRunnableDumpToDatabase {
public:
    virtual ~IRunnableDumpToDatabase(){}
    virtual void runInThreadService() = 0;

};

class IRunnableNetworkCallbacks {
public:
    virtual ~IRunnableNetworkCallbacks(){}
    virtual void runInThreadService() = 0;

};

class IRunnableAsyncNotify {
public:
    virtual ~IRunnableAsyncNotify(){}
    virtual void runInThreadService() = 0;

};

class IRunnableEventLoop {
public:
    virtual ~IRunnableEventLoop(){}
    virtual void runInThreadService() = 0;

};

// -------------------------------------------------
// template for each runnable task
// -------------------------------------------------
template< typename T >
class RunnableTask {
    friend class ThreadedMultitaskService;
public:
    TRunnableClientId addRunnableClient( T * _client ){

        if( ! _client ){
            VS_LOG_ERROR << PRINT_HEADER << " "
                      << RUNNABLE_TASK_NAME
                      << " client for dump to database thread is NULL"
                      << std::endl;
            return INVALID_CLIENT_ID;
        }

        m_muClientsLock.lock();
        const TRunnableClientId clientId = ++m_clientIdGenerator;
        m_taskClients.insert( {clientId, _client} );
        m_muClientsLock.unlock();

        m_cvClientsEvent.notify_one();

        return clientId;
    }

    bool removeRunnableClient( const TRunnableClientId _id ){

        if( _id <= INVALID_CLIENT_ID ){
            VS_LOG_ERROR << PRINT_HEADER << " "
                      << RUNNABLE_TASK_NAME
                      << " invalid client id"
                      << std::endl;
            return false;
        }

        m_muClientsLock.lock();
        const std::unordered_map<TRunnableClientId, IRunnableDumpToDatabase *>::size_type elementsErased = m_taskClients.erase( _id );
        m_muClientsLock.unlock();

        if( 0 == elementsErased ){
            VS_LOG_ERROR << PRINT_HEADER << " "
                      << RUNNABLE_TASK_NAME
                      << " such client id not found [" << _id << "]"
                      << std::endl;
            return false;
        }

        return true;
    }

private:
    RunnableTask( std::string _taskName )
        : RUNNABLE_TASK_NAME(_taskName)
    {
        m_threadTaskProcessing = new std::thread( & RunnableTask::threadTaskProcessing, this );
    }

    ~RunnableTask(){

    }

    RunnableTask( const RunnableTask & _inst ) = delete;
    RunnableTask & operator=( const RunnableTask & _inst ) = delete;

    void threadTaskProcessing(){

        VS_LOG_INFO << PRINT_HEADER << " "
                 << RUNNABLE_TASK_NAME
                 << " thread is STARTED"
                 << std::endl;

        while( ! m_shutdownCalled.load() ){

            VS_LOG_INFO << PRINT_HEADER << " "
                     << RUNNABLE_TASK_NAME
                     << " thread go to SLEEP"
                     << std::endl;

            std::mutex muCvLock;
            std::unique_lock<std::mutex> lock( muCvLock );
            m_cvClientsEvent.wait( lock, [this]{
                m_muClientsLock.lock();
                bool isClientExists = ! m_taskClients.empty();
                m_muClientsLock.unlock();
                return ( isClientExists || m_shutdownCalled.load() );
            } );

            VS_LOG_INFO << PRINT_HEADER << " "
                     << RUNNABLE_TASK_NAME
                     << " thread is WAKED UP"
                     << std::endl;

            if( m_shutdownCalled.load() ){
                break;
            }

            m_muClientsLock.lock();
            bool isClientExists = ! m_taskClients.empty();
            m_muClientsLock.unlock();

            while( isClientExists ){

                m_muClientsLock.lock();
                for( auto & valuePair : m_taskClients ){
                    valuePair.second->runInThreadService();
                }

                isClientExists = ! m_taskClients.empty();
                m_muClientsLock.unlock();
            }
        }

        VS_LOG_INFO << PRINT_HEADER << " "
                 << RUNNABLE_TASK_NAME
                 << " thread go to EXIT"
                 << std::endl;
    }

    const std::string RUNNABLE_TASK_NAME;
    std::unordered_map<TRunnableClientId, T *> m_taskClients;
    std::atomic<bool> m_shutdownCalled;

    TRunnableClientId m_clientIdGenerator;
    std::thread * m_threadTaskProcessing;
    std::mutex m_muClientsLock;
    std::condition_variable m_cvClientsEvent;
};

// -------------------------------------------------
// multitask global service
// -------------------------------------------------
class ThreadedMultitaskService
{
public:
    static ThreadedMultitaskService & singleton(){
        static ThreadedMultitaskService instance;
        return instance;
    }

    bool init();
    void shutdown();

    TRunnableClientId addRunnableClient( IRunnableDumpToDatabase * _client );
    bool removeDumpToDatabaseClient( const TRunnableClientId _id );


private:
    ThreadedMultitaskService();
    ~ThreadedMultitaskService();

    ThreadedMultitaskService( const ThreadedMultitaskService & _inst ) = delete;
    ThreadedMultitaskService & operator=( const ThreadedMultitaskService & _inst ) = delete;

    void threadDumpToDatabase();

    // data
    RunnableTask<IRunnableDumpToDatabase> m_runnableTaskDumpToDatabase;
    RunnableTask<IRunnableNetworkCallbacks> m_runnableTaskNetworkCallbacks;
    RunnableTask<IRunnableStateMonitoring> m_runnableTaskStateMonitoring;
    RunnableTask<IRunnableAsyncNotify> m_runnableTaskAsyncNotify;

    std::unordered_map<TRunnableClientId, IRunnableDumpToDatabase *> m_clientsForDumpToDatabase;

    std::atomic<bool> m_shutdownCalled;

    // service
    TRunnableClientId m_clientIdGenerator;
    std::thread * m_threadDumpToDatabase;
    std::mutex m_muDumpToDatabaseLock;
    std::condition_variable m_cvDumpToDatabaseEvent;

};

}

#define MULTITASK_SERVICE ThreadedMultitaskService::singleton()

#endif // THREADED_MULTITASK_SERVICE_H

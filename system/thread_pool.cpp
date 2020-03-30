
// std
#include <algorithm>
// project
#include "thread_pool.h"

using namespace std;

ThreadPool::ThreadPool( const int _threads )
    : m_terminate(false)
    , m_stopped(false)
{

    m_threadsActivity.resize( _threads );
    m_workersCurrentTask.resize( _threads );

    for( int i = 0; i < _threads; i++ ){

        m_pool.emplace_back( thread( & ThreadPool::Worker, this, i) );
        m_threadsActivity[ i ] = false;
    }
}

void ThreadPool::enqueue( IThreadPoolTask *_task ){

	{
        std::unique_lock<std::mutex> lock(m_inputTasksMutex);

        m_inputTasks.push( _task );
	}

    m_condition.notify_one();
}

void ThreadPool::Worker( int16_t _threadID ){

    IThreadPoolTask * task;

    while( true ){

		{
            std::unique_lock<std::mutex> lock(m_inputTasksMutex);

			// Wait until queue is not empty or termination signal is sent.
            m_condition.wait( lock, [this]{ return !m_inputTasks.empty() || m_terminate; } );

            if( m_terminate && m_inputTasks.empty() ){
				return;
			}

            task = m_inputTasks.front();
            m_inputTasks.pop();
		}

        m_threadsActivity[ _threadID ] = true;
        m_workersCurrentTask[ _threadID ] = task;

        task->processInThread();

        // TODO delete task ?
        m_workersCurrentTask[ _threadID ] = nullptr;
        m_threadsActivity[ _threadID ] = false;
	}
}

void ThreadPool::shutdown(){

	{
        std::unique_lock<std::mutex> lock(m_inputTasksMutex);

        m_terminate = true;
	}

    m_condition.notify_all();

    for( std::thread & thread : m_pool ){
		thread.join();
	}

    m_stopped = true;
}

ThreadPool::~ThreadPool(){

    if( ! m_stopped ){
        shutdown();
	}
}

bool ThreadPool::allTasksDone(){

    std::unique_lock<std::mutex> lock(m_inputTasksMutex);

    auto activeThreadIter = find( m_threadsActivity.begin(), m_threadsActivity.end(), true );

    return ( (activeThreadIter == m_threadsActivity.end()) && (m_inputTasks.empty()) );
}

void ThreadPool::cancelPendingAndProcessTasks(){

    for( int i = 0; m_inputTasks.size(); i++ ){
        m_inputTasks.pop();
    }

    for( int i = 0; m_workersCurrentTask.size(); i++ ){

        if( m_workersCurrentTask[ i ] ){
            IThreadPoolTask * curTask = m_workersCurrentTask[ i ];
            curTask->cancel();
            m_workersCurrentTask[ i ] = nullptr;
        }
    }
}



#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <iostream>
#include <unistd.h>

#include "thread_pool_task.h"

class ThreadPool
{
public:
    ThreadPool( const int _threads );
    ~ThreadPool();

    void shutdown();

    void enqueue( IThreadPoolTask * _task );
    bool allTasksDone();
    void cancelPendingAndProcessTasks();


private:
    void Worker( int16_t _threadID );

    // data
    std::queue<IThreadPoolTask *> m_inputTasks;
    std::vector<IThreadPoolTask *> m_workersCurrentTask;
    std::vector< bool > m_threadsActivity; // TODO std::atomic_bool
    bool m_terminate;
    bool m_stopped;

    // service
    std::vector<std::thread> m_pool;
    std::mutex m_inputTasksMutex;
    std::condition_variable m_condition;

};

#endif // THREAD_POOL_H

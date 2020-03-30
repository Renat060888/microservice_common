#ifndef THREAD_POOL_TASK_H
#define THREAD_POOL_TASK_H

class IThreadPoolTask
{
public:
    IThreadPoolTask( bool _selfDestruction )
        : m_cancelled(false)
        , m_selfDestruction(_selfDestruction)
    {}

    virtual ~IThreadPoolTask(){}

    virtual bool processInThread() = 0;

    void cancel(){ m_cancelled = true; }


private:

    // TODO: do
    bool m_selfDestruction;
    bool m_cancelled;

};

#endif // THREAD_POOL_TASK_H

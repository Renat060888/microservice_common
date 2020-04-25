#ifndef PROCESS_LAUNCHER_H
#define PROCESS_LAUNCHER_H

#include <signal.h>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <atomic>
#include <string>
#include <vector>
#include <queue>
#include <map>
#include <semaphore.h>

// ------------------------------------------------------------------------
// unit
// ------------------------------------------------------------------------
class ProcessHandle {
    friend class ProcessLauncher;
    using TProcessId = pid_t;
    static const int NOT_DEFINED_EXIT_CODE = 9999999;
public:
    enum EProcessRole {
        PARENT,
        CHILD,
        UNDEFINED
    };

    struct SInitSettings {
        SInitSettings()
            : readOuput(false)
        {}
        std::string program;
        std::vector<std::string> args;
        bool readOuput;
    };

    ProcessHandle( SInitSettings _settings );
    ~ProcessHandle();

    TProcessId getChildPid();
    int getExitStatus(){ return m_exitStatusCode; }
    const SInitSettings & getSettings(){ return m_settings; }

    std::string findArgValue( std::string _argKey );
    bool isReadOutput(){ return m_settings.readOuput; }
    bool isRunning();


    // messaging
    void sendToChildStdin( const std::string & _msg );
    bool isMessageFromChildStdoutExist();
    const std::string & readFromChildStdout();

private:
    // control
    void wait();
    void requestForClose( int _signal );
    void kill( int _signal );

    void setRole( EProcessRole _role, TProcessId _id );
    void setExitStatus( int _code );

    TProcessId m_childPid;
    EProcessRole m_role;
    int m_pipeFileDs[ 2 ];
    std::string m_childStdout;
    SInitSettings m_settings;
    bool m_died;
    int m_exitStatusCode;
};

class IProcessObserver {
public:
    IProcessObserver( int32_t _priority )
        : m_priority(_priority)
    {}
    ~IProcessObserver(){}

    virtual void callbackProcessCrashed( ProcessHandle * _handle ) = 0;

    int32_t m_priority;
    std::string m_programNameToObserve;
};

// ------------------------------------------------------------------------
// control
// ------------------------------------------------------------------------
class ProcessLauncher
{
public:
    using TLaunchTaskId = int32_t;
    struct SLaunchTask {
        SLaunchTask()
            : taskId(0)
            , launched(false)
            , readOutput(true)
            , checkForDuplicate(true)
            , handle(nullptr)
        {}
        TLaunchTaskId taskId;
        std::atomic<bool> launched;

        // launch parameters
        std::string program;
        std::vector<std::string> args;
        bool readOutput;
        bool checkForDuplicate;

        // result
        ProcessHandle * handle;
    };

    static ProcessLauncher & singleton(){
        static ProcessLauncher instance;
        return instance;
    }    
    void shutdown();


    // notify
    void addObserver( IProcessObserver * _observer, pid_t _processToObserve );
    void removeObserver( IProcessObserver * _observer );

    // control
    ProcessHandle * launch( const std::string & _program,
                            const std::vector<std::string> & _args,
                            bool _readOutput = true,
                            bool _checkForDuplicate = true );
    const SLaunchTask * addLaunchTask( const std::string & _program,
                            const std::vector<std::string> & _args,
                            bool _readOutput = true,
                            bool _checkForDuplicate = true );
    bool removeLaunchTask( TLaunchTaskId _taskId );
    void kill( ProcessHandle * & _handle, int _signal = SIGKILL );
    void kill( pid_t _pid, int _signal = SIGKILL );
    void close( ProcessHandle * _handle, int _signal = SIGTERM );

    // sync
    bool getLock( std::string _lockName, bool _createLock = false );
    bool releaseLock( std::string _lockName );

private:
    ProcessLauncher();
    ~ProcessLauncher();

    ProcessLauncher( const ProcessLauncher & _inst ) = delete;
    ProcessLauncher & operator=( const ProcessLauncher & _inst ) = delete;

    void threadChildProcessMonitoring();
    void runSystemClock();

    bool isThisMainThread();
    void setChildProcess();

    // data
    std::vector<ProcessHandle *> m_handles;
    std::map<TLaunchTaskId, SLaunchTask *> m_processesLaunchedFromChildThreadTasks;
    std::queue<SLaunchTask *> m_launchTasks;
    bool m_shutdown;
    std::vector<IProcessObserver *> m_observers;
    std::map<std::string, sem_t *> m_processLocks;

    // service
    int32_t m_launchTaskIdGenerator;
    std::thread * m_trChildProcessMonitoring;    
    std::condition_variable m_cvChildProcessMonitoring;
    std::mutex m_muHandlesLock;
    std::mutex m_muLaunchTaskLock;
};
#define PROCESS_LAUNCHER ProcessLauncher::singleton()

#endif // PROCESS_LAUNCHER_H

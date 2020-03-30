
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <unordered_map>

#include "common/ms_common_types.h"
#include "common/ms_common_utils.h"
#include "logger.h"
#include "process_launcher.h"

using namespace std;

bool thread_local g_isMainThread = false;

static constexpr const char * PRINT_HEADER = "ProcessLauncher:";
static constexpr int BYTES_COUNT_READ_FROM_PIPE = 10240;
static constexpr int TIMEOUT_TO_READ_FROM_PIPE_MILLISEC = 100;

static std::mutex g_exitedChildStatusLock;
static std::unordered_map<common_types::TPid, int> g_exitedChildStatus;

void signalChildDieHandler( int sig ){

    // NOTE: complex structures ( containers, mutexes, ... ) NOT allowed - function is called spurious by kernel-signal

    if( SIGCHLD == sig ){
        int childExitStatus = 0;
        const pid_t childPid = ::wait( & childExitStatus );

//        g_exitedChildStatusLock.lock();
        // TODO: here is a misterious bug (O_o)
//        g_exitedChildStatus.insert( {childPid, childExitStatus} );
//        g_exitedChildStatusLock.unlock();

        PRELOG_INFO << PRINT_HEADER
                    << " unix signal handler: child process died, pid [" << childPid << "]"
                    << " status code [" << WEXITSTATUS( childExitStatus )
                    << "] kill zombie..."
                    << endl;
    }
    else{
        PRELOG_INFO << PRINT_HEADER << " unix signal handler: unknown signal [" << sig << "]" << endl;
    }
}

static string convertSignalToStr( int _signal ){

    switch( _signal ){
    case SIGINT: return "SIGINT";
    case SIGABRT: return "SIGABRT";
    case SIGKILL: return "SIGKILL";
    case SIGTERM: return "SIGTERM";
    case SIGSEGV: return "SIGSEGV";
    default: {
        assert( false && "unknown signal to str" );
    }
    }
}

static bool killProcess( common_types::TPid _pid, int _signal ){

    VS_LOG_INFO << PRINT_HEADER
             << " try to kill process with pid [" << _pid << "]"
             << " by signal [" << strsignal(_signal) << "]"
             << endl;

    const int rt = ::kill( _pid, _signal );
    if( rt != 0 ){
        VS_LOG_ERROR << PRINT_HEADER << " killing failed, reason: [" << strerror( errno ) << "]" << endl;
        return false;
    }

    return true;
}

// --------------------------------------------------------------
// handle
// --------------------------------------------------------------
ProcessHandle::ProcessHandle( SInitSettings _settings )
    : m_childPid(0)
    , m_settings(_settings)
    , m_died(false)
    , m_exitStatusCode(NOT_DEFINED_EXIT_CODE)
{
    if( m_settings.readOuput ){
        const int pipeRt = pipe( m_pipeFileDs );
        if( pipeRt < 0 ){
            VS_LOG_CRITICAL << PRINT_HEADER << " pipe() failed" << endl;
            assert( false && "ProcessHandle CTOR fail" );
        }
    }
}

ProcessHandle::~ProcessHandle()
{
    if( m_settings.readOuput ){
        close( m_pipeFileDs[ 0 ] );
        close( m_pipeFileDs[ 1 ] );
    }

    if( ! m_died ){
        // TODO: or close ?
        kill( SIGKILL );
    }
}

ProcessHandle::TProcessId ProcessHandle::getChildPid(){
    return m_childPid;
}

std::string ProcessHandle::findArgValue( std::string _argKey ){

    for( const std::string & argStr : m_settings.args ){
        if( argStr.find(_argKey) != std::string::npos ){
            return common_utils::cutBySimbol( '=', argStr ).second;
        }
    }
    return string();
}

void ProcessHandle::setRole( EProcessRole _role, TProcessId _id ){

    m_childPid = _id;

    if( m_settings.readOuput ){

        if( EProcessRole::PARENT == _role ){

            // parent process will not write to the INPUT ( 1 index ) of pipe
            close( m_pipeFileDs[ 1 ] );
        }
        else if( EProcessRole::CHILD == _role ){

            // child process will not read OUTPUT ( 0 index ) of pipe
            close( m_pipeFileDs[ 0 ] );

            // child process writes itself stdout/stderr to the INPUT ( 1 index ) of pipe
            const int dupOutRt = dup2( m_pipeFileDs[ 1 ], STDOUT_FILENO );
            if( dupOutRt < 0 ){
                VS_LOG_CRITICAL << PRINT_HEADER
                             << " dup2() stdout failed."
                             << " Reason: " << strerror( errno )
                             << endl;
                return;
            }

            const int dupErrRt = dup2( m_pipeFileDs[ 1 ], STDERR_FILENO );
            if( dupErrRt < 0 ){
                VS_LOG_CRITICAL << PRINT_HEADER
                             << " dup2() stderr failed."
                             << " Reason: " << strerror( errno )
                             << endl;
                return;
            }

            // after copy not any more needed
            close( m_pipeFileDs[ 1 ] );

            // close inherited from the parent file descriptors ( except 0, 1, 2 - stdout, stdin, stderr )
            for( int i = 3; i < sysconf(_SC_OPEN_MAX); i++ ){
                close( i );
            }
        }
        else{
            assert( false && "incorrect process role" );
        }
    }
}

void ProcessHandle::setExitStatus( int _code ){
    m_exitStatusCode = _code;
}

void ProcessHandle::wait(){

    // TODO: do with timeout
//    ::wait()
}

bool ProcessHandle::isRunning(){

    if( 0 == ::kill(m_childPid, 0) ){
        return true;
    }
    else{
        return false;
        m_died = true;
    }
}

void ProcessHandle::requestForClose( int _signal ){

    VS_LOG_INFO << PRINT_HEADER
             << " try to close process with pid [" << m_childPid << "]"
             << " by signal [" << strsignal(_signal) << "]"
             << endl;

    const int rt = ::kill( m_childPid, _signal );
    if( rt != 0 ){
        VS_LOG_ERROR << "closing failed, reason: [" << strerror( errno ) << "]" << endl;
    }
}

void ProcessHandle::kill( int _signal ){

    killProcess( m_childPid, _signal );
}

void ProcessHandle::sendToChildStdin( const std::string & /*_msg*/ ){

    // TODO: may be later
}

bool ProcessHandle::isMessageFromChildStdoutExist(){

    if( m_settings.readOuput ){
        // non-blocking accept
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = TIMEOUT_TO_READ_FROM_PIPE_MILLISEC * 1000; // mul to micro
        fd_set readfds;
        fcntl( m_pipeFileDs[ 0 ], F_SETFL, O_NONBLOCK );
        FD_ZERO( & readfds );
        FD_SET( m_pipeFileDs[ 0 ], & readfds );
        int readyDescrCount = select( m_pipeFileDs[ 0 ] + 1 , & readfds, NULL, NULL, & timeout );
        if( readyDescrCount != 1 ){
            return false;
        }

        // try to read from the child stdout
        char buf[ BYTES_COUNT_READ_FROM_PIPE ];
        const int readedBytesCount = read( m_pipeFileDs[ 0 ], buf, sizeof(buf) );

        if( readedBytesCount > 0 ){
            m_childStdout.assign( buf, readedBytesCount );
            return true;
        }
    }

    return false;
}

const std::string & ProcessHandle::readFromChildStdout(){
    return m_childStdout;
}

// --------------------------------------------------------------
// launcher
// --------------------------------------------------------------
ProcessLauncher::ProcessLauncher()
    : m_shutdown(false)
    , m_trChildProcessMonitoring(nullptr)
    , m_launchTaskIdGenerator(0)
{
    g_isMainThread = true; // NOTE: because ProcessLauncher is a singleton and will be created in main thread at start

    sigset( SIGCHLD, & signalChildDieHandler );

    m_trChildProcessMonitoring = new std::thread( & ProcessLauncher::threadChildProcessMonitoring, this );
}

ProcessLauncher::~ProcessLauncher()
{
    shutdown();
}

void ProcessLauncher::runSystemClock(){

    m_muLaunchTaskLock.lock();
    if( ! m_launchTasks.empty() ){
        SLaunchTask * task = m_launchTasks.back();
        m_launchTasks.pop();

        VS_LOG_INFO << PRINT_HEADER << " found process task [" << task->taskId << "]. Launch it" << endl;

        task->handle = launch( task->program, task->args, task->readOutput, task->checkForDuplicate );
        task->launched.store( true );
    }
    m_muLaunchTaskLock.unlock();
}

void ProcessLauncher::shutdown(){

    if( ! m_shutdown ){
        VS_LOG_INFO << PRINT_HEADER << " begin shutdown" << endl;

        m_muHandlesLock.lock();
        for( ProcessHandle * process : m_handles ){
            delete process;
        }
        m_handles.clear();
        m_muHandlesLock.unlock();

        m_shutdown = true;
        m_cvChildProcessMonitoring.notify_one();
        common_utils::threadShutdown( m_trChildProcessMonitoring );

        for( auto & valuePair : m_processLocks ){
            const string & semName = valuePair.first;
            sem_t * semaphore = valuePair.second;
            const int rt = sem_close( semaphore );
            // TODO: ?
//            const int rt = sem_unlink( semName.c_str() );
        }

        VS_LOG_INFO << PRINT_HEADER << " shutdown success" << endl;
    }
}

struct SProcessObserverComparator {
    bool operator()( IProcessObserver * _lhs, IProcessObserver * _rhs ){
        return _lhs->m_priority < _rhs->m_priority;
    }
};

void ProcessLauncher::addObserver( IProcessObserver * _observer ){

    // TODO: check for duplicate

    assert( _observer );

    m_observers.push_back( _observer );

    // for callbacks in priority order
    std::sort( m_observers.begin(), m_observers.end(), SProcessObserverComparator() );
}

void ProcessLauncher::removeObserver( IProcessObserver * _observer ){

    for( auto iter = m_observers.begin(); iter != m_observers.end(); ){
        IProcessObserver * observer = ( * iter );
        if( observer == _observer ){
            iter = m_observers.erase( iter );
            return;
        }
        else{
            ++iter;
        }
    }
}

ProcessHandle * ProcessLauncher::launch( const std::string & _program,
                                         const std::vector<std::string> & _args,
                                         bool _readOutput,
                                         bool _checkForDuplicate ){

    // TODO: check for program file exist

    // TODO: examine this once more
//    assert( isThisMainThread() && "process launch in child threads now is prohibited" );

    // check that process with same args already launched
    if( _checkForDuplicate ){
        m_muHandlesLock.lock();
        for( ProcessHandle * process : m_handles ){

            if( process->getSettings().program == _program ){
                if( process->getSettings().args == _args ){
                    VS_LOG_ERROR << "such process with same args already launched: [" << _program
                             << "] Or set flag to allow duplicates"
                             << endl;
                    m_muHandlesLock.unlock();
                    return nullptr;
                }
            }
        }
        m_muHandlesLock.unlock();
    }

    VS_LOG_INFO << endl;
    VS_LOG_INFO << PRINT_HEADER << " try to launch a following process [" << _program << "]" << endl;
    for( const string & arg : _args ){
        VS_LOG_INFO << " " << arg;
    }
    VS_LOG_INFO << endl;

    // NOTE: allocate must be done before fork()
    char wdPath[ PATH_MAX ];
    char * cwdBuf = getcwd( wdPath, PATH_MAX );
    const string result( cwdBuf );
    const string programFullPath = result + "/" + _program;

    vector<char *> argv( _args.size() + 2 ); // 2 - program name + NULL
    int i = 0;
    argv[ i++ ] = const_cast<char*>( programFullPath.c_str() );
    for( auto iter = _args.begin(); iter != _args.end(); ++iter ){
        argv[ i++ ] = const_cast<char*>( (* iter).c_str() );
    }
    argv[ i ] = NULL;

    // TODO: copy environment variables

    ProcessHandle::SInitSettings sets;
    sets.program = _program;
    sets.args = _args;
    sets.readOuput = _readOutput;
    ProcessHandle * handle = new ProcessHandle( sets );

    m_muHandlesLock.lock();
    m_handles.push_back( handle );
    m_muHandlesLock.unlock();

    m_cvChildProcessMonitoring.notify_one();

    VS_LOG_INFO << PRINT_HEADER << " initiate fork..." << endl;

    // ---------------------------------
    // (I) copy this process
    // ---------------------------------
    const pid_t forkRt = fork();
    if( forkRt < 0 ){
        VS_LOG_CRITICAL << PRINT_HEADER << " fork() failed" << endl;
        return nullptr;
    }
    else if( forkRt > 0 ){
//        LOG_TRACE << PRINT_HEADER
//                 << " fork() success, parent process is continues with pid " << getpid()
//                 << endl;

        handle->setRole( ProcessHandle::EProcessRole::PARENT, forkRt );
        return handle;
    }
    else{
//        LOG_TRACE << PRINT_HEADER
//                 << " fork() success, child process is started with pid " << getpid()
//                 << " Begin execv()..."
//                 << endl;

        handle->setRole( ProcessHandle::EProcessRole::CHILD, getpid() );

        // TODO: restore environment variables

        // ---------------------------------
        // (II) replace by new binary image
        // ---------------------------------
        const int execRt = execv( argv[ 0 ], & argv[ 0 ] );
        if( execRt < 0 ){
            VS_LOG_CRITICAL << PRINT_HEADER
                         << " execv() with [" << argv[ 0 ] << "] failed."
                         << " Reason: " << strerror( errno )
                         << endl;
            return nullptr;
        }

        return handle;
    }

    return nullptr;
}

const ProcessLauncher::SLaunchTask * ProcessLauncher::addLaunchTask( const std::string & _program,
                                                const std::vector<std::string> & _args,
                                                bool _readOutput,
                                                bool _checkForDuplicate ){

    // TODO: examine this once more
//    assert( ! isThisMainThread() && "this is main thread - just use regular launch()" );

    SLaunchTask * task = new SLaunchTask();
    task->program = _program;
    task->args = _args;
    task->readOutput = _readOutput;
    task->checkForDuplicate = _checkForDuplicate;

    task->taskId = ++m_launchTaskIdGenerator;
    task->launched.store( false );

    m_muLaunchTaskLock.lock();
    m_processesLaunchedFromChildThreadTasks.insert( {task->taskId, task} );
    m_launchTasks.push( task );
    m_muLaunchTaskLock.unlock();

    m_cvChildProcessMonitoring.notify_one();

    return task;
}

bool ProcessLauncher::removeLaunchTask( TLaunchTaskId _taskId ){

    m_muLaunchTaskLock.lock();
    auto iter = m_processesLaunchedFromChildThreadTasks.find( _taskId );
    if( iter != m_processesLaunchedFromChildThreadTasks.end() ){
        SLaunchTask * task = iter->second;
        delete task;
        m_processesLaunchedFromChildThreadTasks.erase( iter );
        m_muLaunchTaskLock.unlock();
        return true;
    }
    m_muLaunchTaskLock.unlock();

    VS_LOG_WARN << PRINT_HEADER << " such task id not found [" << _taskId << "]" << endl;
    return false;
}

void ProcessLauncher::kill( ProcessHandle * & _handle, int _signal ){

    if( ! _handle ){
        return;
    }

    m_muHandlesLock.lock();
    for( auto iter = m_handles.begin(); iter != m_handles.end(); ){
        ProcessHandle * process = ( * iter );

        if( process->getChildPid() == _handle->getChildPid() ){
            process->kill( _signal );
            iter = m_handles.erase( iter );
            _handle = nullptr;

            m_muHandlesLock.unlock();
            return;
        }
        else{
            ++iter;
        }
    }
    m_muHandlesLock.unlock();
}

void ProcessLauncher::kill( pid_t _pid, int _signal ){

    killProcess( _pid, _signal );
}

void ProcessLauncher::close( ProcessHandle * _handle, int _signal ){

    if( ! _handle ){
        return;
    }

    m_muHandlesLock.lock();
    for( auto iter = m_handles.begin(); iter != m_handles.end(); ){
        ProcessHandle * process = ( * iter );

        if( process->getChildPid() == _handle->getChildPid() ){
            process->requestForClose( _signal );
            iter = m_handles.erase( iter );
            _handle = nullptr;

            m_muHandlesLock.unlock();
            return;
        }
        else{
            ++iter;
        }
    }
    m_muHandlesLock.unlock();
}

void ProcessLauncher::threadChildProcessMonitoring(){

    while( ! m_shutdown ){

        // sleep thread if no handles
        std::mutex muCvLock;
        std::unique_lock<std::mutex> lock( muCvLock );
        m_cvChildProcessMonitoring.wait( lock, [this]{
            m_muHandlesLock.lock();
            bool childsExist = ( ! m_handles.empty() || ! m_launchTasks.empty() );
            m_muHandlesLock.unlock();
            return ( childsExist || m_shutdown );
        } );

        if( m_shutdown ){            
            break;
        }

        VS_LOG_INFO << PRINT_HEADER
                 << " child process monitoring THREAD is waked up"
                 << endl;

        m_muHandlesLock.lock();
        bool childsExist = ( ! m_handles.empty() || ! m_launchTasks.empty() );
        m_muHandlesLock.unlock();

        while( childsExist ){

            runSystemClock();

            // read childs output
            m_muHandlesLock.lock();
            std::vector<ProcessHandle *> diedProcesses;

            for( ProcessHandle * process : m_handles ){

                if( process->isReadOutput() ){
                    const bool isMessageExist = process->isMessageFromChildStdoutExist();
                    if( isMessageExist ){
                        VS_LOG_INFO << "ChildProcess [" << process->getChildPid() << "] => "
                                 << process->readFromChildStdout()
                                 << std::flush;
                    }
                }

                if( ! process->isRunning() ){
                    VS_LOG_WARN << PRINT_HEADER << " process crashed, pid = " << process->getChildPid() << endl;

                    if( g_exitedChildStatusLock.try_lock() ){
                        auto iter = g_exitedChildStatus.find( process->getChildPid() );
                        if( iter != g_exitedChildStatus.end() ){
                            process->setExitStatus( iter->second );
                        }
                        g_exitedChildStatusLock.unlock();
                    }

                    diedProcesses.push_back( process );
                }
            }

            // clean died processes from currents
            if( ! diedProcesses.empty() ){
                for( auto iter = m_handles.begin(); iter != m_handles.end(); ){
                    ProcessHandle * proc = ( * iter );

                    bool processDied = false;
                    for( ProcessHandle * diedProcess : diedProcesses ){
                        if( diedProcess->getChildPid() == proc->getChildPid() ){
                            iter = m_handles.erase( iter );
                            processDied = true;
                            break;
                        }
                    }

                    if( ! processDied ){
                        ++iter;
                    }
                }
            }

            // check for handles eliminating
            childsExist = ! m_handles.empty();
            m_muHandlesLock.unlock();

            // notify about zombies
            // TODO: delete zombies ?
            for( ProcessHandle * zombie : diedProcesses ){
                for( IProcessObserver * observer : m_observers ){
                    observer->callbackProcessCrashed( zombie );
                }
            }

            // this helps mutex locking by other sections
            this_thread::sleep_for( chrono::milliseconds(5) );
        }

        VS_LOG_INFO << PRINT_HEADER
                 << " child process monitoring THREAD is going to sleep"
                 << endl;
    }

    VS_LOG_INFO << PRINT_HEADER
             << " child process monitoring THREAD is exit"
             << endl;
}

bool ProcessLauncher::isThisMainThread(){

    // NOTE: gettid() support only on GLibc >= 2.30
//    return ( getpid() == gettid() );

#ifdef SYS_gettid
    const pid_t pid = getpid();
    const pid_t tid = syscall( SYS_gettid );
    return ( pid == tid );
#else
    return g_isMainThread;
#endif
}

// --------------------------------------------------------------
// sync
// --------------------------------------------------------------
bool ProcessLauncher::getLock( std::string _lockName, bool _createLock ){

    auto iter = m_processLocks.find( _lockName );
    if( iter != m_processLocks.end() ){
        VS_LOG_WARN << PRINT_HEADER << " lock [" << _lockName << "] already getted" << endl;
        return true;
    }
    else{
        sem_t * semaphore = nullptr;
        if( _createLock ){
            constexpr int semCounter = 1; // mutex mode
            constexpr int umask = 0660;

            semaphore = ::sem_open( _lockName.c_str(), O_CREAT, umask, semCounter );
            if( ! semaphore ){
                VS_LOG_ERROR << PRINT_HEADER << " lock with create [" << _lockName << "] get failed. Reason [" << strerror( errno ) << "]" << endl;
                return false;
            }
        }
        else{
            semaphore = ::sem_open( _lockName.c_str(), 0 );
            if( ! semaphore ){
                VS_LOG_ERROR << PRINT_HEADER << " lock w/o create [" << _lockName << "] get failed. Reason [" << strerror( errno ) << "]" << endl;
                return false;
            }
        }

        const int rt = ::sem_wait( semaphore );
        if( rt < 0 ){
            VS_LOG_WARN << PRINT_HEADER << " something wrong with sem_wait() [" << _lockName << "]. Reason [" << strerror( errno ) << "]" << endl;
        }

        m_processLocks.insert( {_lockName, semaphore} );
        return true;
    }
}

bool ProcessLauncher::releaseLock( std::string _lockName ){

    auto iter = m_processLocks.find( _lockName );
    if( iter != m_processLocks.end() ){
        sem_t * semaphore = iter->second;

        const int rt = ::sem_post( semaphore );
        if( rt < 0 ){
            VS_LOG_WARN << PRINT_HEADER << " something wrong with sem_post() [" << _lockName << "]. Reason [" << strerror( errno ) << "]" << endl;
        }

        return true;
    }
    else{
        VS_LOG_ERROR << PRINT_HEADER << " such semaphore not found [" << _lockName << "]" << endl;
        return false;
    }
}















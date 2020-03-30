
#include <iostream>
#include <cassert>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>

#include <boost/filesystem.hpp>

#include "system/object_pool.h"
#include "common/ms_common_utils.h"
#include "system/logger.h"
#include "shell.h"

using namespace std;

static constexpr const char * PRINT_HEADER = "Shell-Network:";
static constexpr const char * CODE_WORD_TO_EXIT = "unicorn";
static constexpr const int32_t BYTES_COUNT_READ_FROM_SOCKET = 1024 * 100; // 100 kb

// TODO: клиент не доработан на 30-40 процентов !
// Только для соединения точка-точка с ОДНИМ клиентом

// -----------------------------------------------------------------------------
// request override
// -----------------------------------------------------------------------------
class ShellRequest : public AEnvironmentRequest {

public:
    ShellRequest()        
    {
        clear();
    }

    virtual void setOutcomingMessage( const std::string & _msg ) override {

        assert( clientSocketDscr > 0 && "client socket descr error - connection must be established" );

        // client mode: request & immediate response from server
        if( clientModeInitiative ){
            interface->m_sendProxy( clientSocketDscr, _msg );
            if( ! AEnvironmentRequest::m_asyncRequest ){
                const string response = interface->m_receiveProxy( clientSocketDscr );
                m_incomingMessage = response;
            }
        }
        // server mode: response to client
        else{
            interface->m_sendProxy( clientSocketDscr, _msg );
            // TODO: справедливо для частного случая ( запуска шелл клиента для одинарной команды ) , а в остальных ситуациях?
//            ::shutdown( clientSocketDscr, SHUT_RDWR );
//            ::close( clientSocketDscr );
        }
    }

    // TODO: may be virtual ?
    void clear(){
        clientSocketDscr = 0;
        clientModeInitiative = false;
        interface = nullptr;
    }

    int clientSocketDscr;
    bool clientModeInitiative;
    Shell * interface;
};
using PShellRequest = std::shared_ptr<ShellRequest>;

//
struct SPrivateImpl {
    ObjectPool<ShellRequest> poolOfRequests;
};


//
Shell::Shell( INetworkEntity::TConnectionId _id )
    : INetworkProvider(_id)
    , INetworkClient(_id)
    , m_threadClientAccepting(nullptr)
    , m_threadAsyncClientMode(nullptr)
    , m_shutdownCalled(false)
    , m_clientSocketDscr(0)
    , m_serverSocketDscr(0)
    , m_connectionEstablished(false)
{

}

Shell::~Shell()
{
    shutdown();
}

bool Shell::init( SInitSettings _settings ){

    m_settings = _settings;
    m_privateImpl = new SPrivateImpl();

    switch( _settings.shellMode ){
    case EShellMode::CLIENT : {
        if( ! initClient() ){
            return false;
        }

        switch( _settings.messageMode ){
        case EMessageMode::WITHOUT_SIZE : {
            m_sendProxy = std::bind( & Shell::sendWithoutSize, this, std::placeholders::_1, std::placeholders::_2 );
            m_receiveProxy = std::bind( & Shell::receiveWithoutSize, this, std::placeholders::_1 );
            if( m_settings.asyncClientModeRequests ){
                m_threadAsyncClientMode = new std::thread( & Shell::threadAsyncClientModeRequests, this );
            }
            break;
        }
        case EMessageMode::WITH_SIZE : {
            m_sendProxy = std::bind( & Shell::sendWithSize, this, std::placeholders::_1, std::placeholders::_2 );
            m_receiveProxy = std::bind( & Shell::receiveWithSize, this, std::placeholders::_1 );
            if( m_settings.asyncClientModeRequests ){
                m_threadAsyncClientMode = new std::thread( & Shell::threadAsyncClientModeRequestsWithSize, this );
//                m_threadAsyncClientMode = new std::thread( & Shell::threadSecondTry, this );
            }
            break;
        }
        default : {
            assert( false && "unknown message mode" );
        }
        }

        break;
    }
    case EShellMode::SERVER : {
        if( ! initServer() ){
            return false;
        }

        // TODO: recall why ?
        assert( EMessageMode::WITHOUT_SIZE == _settings.messageMode && "messages in server mode only without-size" );

        m_sendProxy = std::bind( & Shell::sendWithoutSize, this, std::placeholders::_1, std::placeholders::_2 );

        if( m_settings.asyncServerMode ){
            m_threadClientAccepting = new std::thread( & Shell::threadClientAccepting, this );
        }
        break;
    }
    default : {
        assert( false && "unknown shell mode" );
    }
    }       

    return true;
}

bool Shell::isConnectionEstablished(){

    return m_connectionEstablished;
}

void Shell::shutdown(){

    m_observerLock.lock();
    m_observers.clear();
    m_observerLock.unlock();

    m_shutdownCalled = true;
    common_utils::threadShutdown( m_threadClientAccepting );
    common_utils::threadShutdown( m_threadAsyncClientMode );

    if( EShellMode::CLIENT == m_settings.shellMode ){
        ::shutdown( m_clientSocketDscr, SHUT_RDWR );
        ::close( m_clientSocketDscr );

        VS_LOG_INFO << PRINT_HEADER << " client shutdown with socket [" << m_settings.socketFileName << "]" << endl;
    }

    if( EShellMode::SERVER == m_settings.shellMode ){
        ::shutdown( m_clientSocketDscr, SHUT_RDWR );
        ::close( m_clientSocketDscr );
        ::shutdown( m_serverSocketDscr, SHUT_RDWR );
        ::close( m_serverSocketDscr );

        VS_LOG_INFO << PRINT_HEADER << " server shutdown with socket [" << m_settings.socketFileName << "]" << endl;

        try {
            boost::filesystem::remove( m_settings.socketFileName );
        } catch( const boost::filesystem::filesystem_error & _err ){
            VS_LOG_WARN << "boost exception, WHAT: " << _err.what() << " CODE MSG: " << _err.code().message() << endl;
        }
    }

    delete m_privateImpl;
    m_privateImpl = nullptr;
}

void Shell::addObserver( INetworkObserver * _observer ){

    // TODO: check for duplicate

    m_observerLock.lock();
    m_observers.push_back( _observer );
    m_observerLock.unlock();
}

void Shell::removeObserver( INetworkObserver * _observer ){

    m_observerLock.lock();
    for( auto iter = m_observers.begin(); iter != m_observers.end(); ){
        INetworkObserver * observer = ( * iter );

        if( observer == _observer ){
            iter = m_observers.erase( iter );
            m_observerLock.unlock();
            return;
        }
        else{
            ++iter;
        }
    }
    m_observerLock.unlock();
}

void Shell::sendWithoutSize( int _socketDescr, std::string _msg ){

    // TODO: error catching

    // send single message
    const int writedBytesCount = send( _socketDescr, _msg.c_str(), _msg.size(), 0 );
    if( writedBytesCount != (int)_msg.size() ){
        if( writedBytesCount > 0 ){
            VS_LOG_ERROR << "unix-socket-client partial write [" << m_settings.socketFileName << "]" << endl;
        }
        else {
            VS_LOG_ERROR << "unix-socket-client write error [" << m_settings.socketFileName << "]"
                      << " Reason [" << strerror( errno ) << "]"
                      << endl;
        }
    }
}

void Shell::sendWithSize( int _socketDescr, std::string _msg ){

    // TODO: error catching

    const int messageSize = _msg.size();

    // send first size
    int writedBytesCount = send( _socketDescr, & messageSize, sizeof(int), 0 );
    if( writedBytesCount != sizeof(int) ){
        if( writedBytesCount > 0 ){
            VS_LOG_ERROR << "unix-socket-client partial write [" << m_settings.socketFileName << "]" << endl;
        }
        else {
            VS_LOG_ERROR << "unix-socket-client write error [" << m_settings.socketFileName << "]"
                      << " Reason [" << strerror( errno ) << "]"
                      << endl;
        }
    }

    // send next payload
    writedBytesCount = send( _socketDescr, _msg.c_str(), _msg.size(), 0 );
    if( writedBytesCount != (int)_msg.size() ){
        if( writedBytesCount > 0 ){
            VS_LOG_ERROR << "unix-socket-client partial write [" << m_settings.socketFileName << "]" << endl;
        }
        else {
            VS_LOG_ERROR << "unix-socket-client write error [" << m_settings.socketFileName << "]"
                      << " Reason [" << strerror( errno ) << "]"
                      << endl;
        }
    }
}

std::string Shell::receiveWithoutSize( int _socketDescr ){

    char buf[ BYTES_COUNT_READ_FROM_SOCKET ];
    int readedBytesCount = recv( _socketDescr, buf, sizeof(buf), 0 );

    string fromServer;
    if( readedBytesCount >= 0 ){
        fromServer.assign( buf, readedBytesCount );
    }
    else{
        VS_LOG_ERROR << "recv() failed. Reason [" << strerror( errno )
                  << "] Exit from thread [" << m_settings.socketFileName << "]"
                  << endl;
        m_connectionEstablished = false;
    }

    return fromServer;
}

std::string Shell::receiveWithSize( int _socketDescr ){

    // TODO: error catching

    // receive first size
    int messageSize = 0;
    int readedBytesCount = recv( _socketDescr, & messageSize, sizeof(int), 0 );

    // next payload itself
    char buf[ messageSize ];
    readedBytesCount = recv( _socketDescr, buf, sizeof(buf), 0 );
    const string fromServer( buf, readedBytesCount );

    return fromServer;
}

void Shell::threadClientAccepting(){

    while( ! m_shutdownCalled ){
        runNetworkCallbacks();
    }
}

void Shell::threadAsyncClientModeRequests(){

    while( ! m_shutdownCalled ){

        // non-blocking accept
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = m_settings.serverPollTimeoutMillisec * 1000;
        fd_set readfds;
        fcntl( m_clientSocketDscr, F_SETFL, O_NONBLOCK );
        FD_ZERO( & readfds );
        FD_SET( m_clientSocketDscr, & readfds );
        const int readyDescrCount = select( m_clientSocketDscr + 1 , & readfds, NULL, NULL, & timeout );

        if( -1 == readyDescrCount ){
            VS_LOG_ERROR << "select() failed. Reason [" << strerror( errno )
                      << "] Exit from thread [" << m_settings.socketFileName << "]"
                      << endl;
            m_connectionEstablished = false;
            return;
        }

        if( readyDescrCount != 1 ){
            continue;
        }

        // try read from client-mode socket
        char buf[ BYTES_COUNT_READ_FROM_SOCKET ];
        const int readedBytesCount = recv( m_clientSocketDscr, buf, sizeof(buf), MSG_WAITALL );

        string fromServer;
        if( readedBytesCount >= 0 ){
            fromServer.assign( buf, readedBytesCount );
        }
        else{
            VS_LOG_ERROR << "recv() failed. Reason [" << strerror( errno )
                      << "] Exit from thread [" << m_settings.socketFileName << "]"
                      << endl;
            m_connectionEstablished = false;
            return;
        }

        // notify observers
        PShellRequest request = m_privateImpl->poolOfRequests.getInstance();
        request->clientSocketDscr = m_clientSocketDscr;
        request->m_incomingMessage = fromServer;
        request->interface = this;

        // TODO: think about this
        request->m_notifyAboutAsyncViaCallback = true;

        if( request->m_notifyAboutAsyncViaCallback ){
            m_observerLock.lock();
            for( INetworkObserver * observer : m_observers ){
                observer->callbackNetworkRequest( request );
            }
            m_observerLock.unlock();
        }
    }
}

void Shell::threadSecondTry(){

    int length = 0;
    while( ! m_shutdownCalled )
    {
        //lockRead();
        const int rdst = recv( m_clientSocketDscr, &length, sizeof( length ), MSG_WAITALL );

        if (rdst == 0)
        {
            //unlockRead();
            continue;
        }

        char data[1024] = {0};
        std::string text = "";
        int rcv_bytes = 0;
        if( length > 1024 )
        {
            std::cerr << "Big length: " << length << std::endl;
            //unlockRead();
            continue;
        }

        while( rcv_bytes < length )
        {
            rcv_bytes += recv( m_clientSocketDscr, data, length, MSG_WAITALL );
            text += data;
        }
        //unlockRead();

        // notify observers
        PShellRequest request = m_privateImpl->poolOfRequests.getInstance();
        request->clientSocketDscr = m_clientSocketDscr;
        request->m_incomingMessage = text;
        request->interface = this;

        // TODO: think about this
        request->m_notifyAboutAsyncViaCallback = true;

        if( request->m_notifyAboutAsyncViaCallback ){
            m_observerLock.lock();
            for( INetworkObserver * observer : m_observers ){
                observer->callbackNetworkRequest( request );
            }
            m_observerLock.unlock();
        }
    }
}

void Shell::threadAsyncClientModeRequestsWithSize(){

    while( ! m_shutdownCalled ){

        // -------------------------------------------
        // size
        // -------------------------------------------
        int messageSize = 0;
        {
            // non-blocking accept
            struct timeval timeout;
            timeout.tv_sec = 0;
            timeout.tv_usec = m_settings.serverPollTimeoutMillisec * 1000;
            fd_set readfds;
            fcntl( m_clientSocketDscr, F_SETFL, O_NONBLOCK );
            FD_ZERO( & readfds );
            FD_SET( m_clientSocketDscr, & readfds );
            const int readyDescrCount = select( m_clientSocketDscr + 1 , & readfds, NULL, NULL, & timeout );

            if( -1 == readyDescrCount ){
                VS_LOG_ERROR << "select() on 'size' failed. Reason [" << strerror( errno )
                          << "] Exit from thread [" << m_settings.socketFileName << "]"
                          << endl;
                m_connectionEstablished = false;
                return;
            }

            if( readyDescrCount != 1 ){
                continue;
            }

            // try read from client-mode socket
            const int readedBytesCount = recv( m_clientSocketDscr, & messageSize, sizeof(messageSize), MSG_WAITALL );

            if( -1 == readedBytesCount ){
                VS_LOG_ERROR << "recv() on 'size' failed. Reason [" << strerror( errno )
                          << "] Exit from thread [" << m_settings.socketFileName << "]"
                          << endl;
                m_connectionEstablished = false;
                return;
            }
        }

        if( messageSize > 1024 * 1024 * 10 ){ // 10 Kb
            VS_LOG_ERROR << PRINT_HEADER
                         << " message size is greater than 10 Kb = " << messageSize
                         << " Abort receiving"
                         << endl;
            continue;
        }

        // -------------------------------------------
        // payload
        // -------------------------------------------
        string fromServer;
        {
            // non-blocking accept
            struct timeval timeout;
            timeout.tv_sec = 0;
            timeout.tv_usec = m_settings.serverPollTimeoutMillisec * 1000;
            fd_set readfds;
            fcntl( m_clientSocketDscr, F_SETFL, O_NONBLOCK );
            FD_ZERO( & readfds );
            FD_SET( m_clientSocketDscr, & readfds );
            const int readyDescrCount = select( m_clientSocketDscr + 1 , & readfds, NULL, NULL, & timeout );

            if( -1 == readyDescrCount ){
                VS_LOG_ERROR << "select() on 'payload' failed. Reason [" << strerror( errno )
                          << "] Exit from thread [" << m_settings.socketFileName << "]"
                          << endl;
                m_connectionEstablished = false;
                return;
            }

            if( readyDescrCount != 1 ){
                continue;
            }

            // try read from client-mode socket
            int readedBytesCount = 0;
            char buf[ messageSize ];
            while( readedBytesCount < messageSize ){
                readedBytesCount += recv( m_clientSocketDscr, buf, sizeof(buf), MSG_WAITALL );
                const string temp( buf, readedBytesCount );
                fromServer.append( temp );
    //            VS_LOG_TRACE << PRINT_HEADER << " received bytes: " << readedBytesCount << endl;
            }

//            if( readedBytesCount >= 0 ){
//                fromServer.assign( buf, readedBytesCount );
//            }
//            else{
            if( readedBytesCount < 0 ){
                VS_LOG_ERROR << "recv() on 'payload' failed. Reason [" << strerror( errno )
                          << "] Exit from thread [" << m_settings.socketFileName << "]"
                          << endl;
                m_connectionEstablished = false;
                return;
            }
        }

        // notify observers
        PShellRequest request = m_privateImpl->poolOfRequests.getInstance();
        request->clientSocketDscr = m_clientSocketDscr;
        request->m_incomingMessage = fromServer;
        request->interface = this;

        // TODO: think about this
        request->m_notifyAboutAsyncViaCallback = true;

        if( request->m_notifyAboutAsyncViaCallback ){
            m_observerLock.lock();
            for( INetworkObserver * observer : m_observers ){
                observer->callbackNetworkRequest( request );
            }
            m_observerLock.unlock();
        }
    }
}

void Shell::setPollTimeout( int32_t _timeoutMillsec ){
    m_settings.serverPollTimeoutMillisec = _timeoutMillsec;
}

void Shell::runNetworkCallbacks(){

    // for accept
    int clientSocketDscr = 0;

    if( 0 == m_clientSocketDscr ){
        // non-blocking accept
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = m_settings.serverPollTimeoutMillisec * 1000;
        fd_set readfds;
        fcntl( m_serverSocketDscr, F_SETFL, O_NONBLOCK );
        FD_ZERO( & readfds );
        FD_SET( m_serverSocketDscr, & readfds );
        int readyDescrCount = select( m_serverSocketDscr + 1 , & readfds, NULL, NULL, & timeout );
        if( readyDescrCount != 1 ){
            return;
        }

        // accept
        if( (clientSocketDscr = accept(m_serverSocketDscr, NULL, NULL)) == -1 ){
            VS_LOG_ERROR << "accept error. Reason [" << strerror( errno ) << "]" << endl;
            return;
        }
        m_clientSocketDscr = clientSocketDscr;
    }
    else{
        // non-blocking read
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = m_settings.serverPollTimeoutMillisec * 1000;
        fd_set readfds;
        fcntl( m_clientSocketDscr, F_SETFL, O_NONBLOCK );
        FD_ZERO( & readfds );
        FD_SET( m_clientSocketDscr, & readfds );
        int readyDescrCount = select( m_clientSocketDscr + 1 , & readfds, NULL, NULL, & timeout );
        if( readyDescrCount != 1 ){
            return;
        }

        clientSocketDscr = m_clientSocketDscr;
    }

    // read
    char buf[ BYTES_COUNT_READ_FROM_SOCKET ];
    int readedBytesCount = 0;
    if( (readedBytesCount = recv(clientSocketDscr,buf,sizeof(buf),0)) > 0 ){

        const string msg( buf, readedBytesCount );

        // notify observers
        PShellRequest request = std::make_shared<ShellRequest>();
        request->clientSocketDscr = clientSocketDscr;
        request->m_incomingMessage = msg;
        request->interface = this;
        request->m_connectionId = INetworkEntity::getConnId();

        m_observerLock.lock();
        for( INetworkObserver * observer : m_observers ){
            observer->callbackNetworkRequest( request );
        }
        m_observerLock.unlock();
    }

    // check for error
    if( readedBytesCount == -1 ){
        VS_LOG_ERROR << "unix-socket-server read error [" << m_settings.socketFileName << "]" << endl;
        constexpr int waitForSec = 1;
        VS_LOG_ERROR << "wait for [" << waitForSec << "] sec" << endl;
        std::this_thread::sleep_for( std::chrono::seconds(waitForSec) );
    }
    else if( readedBytesCount == 0 ){
        VS_LOG_ERROR << "unix-socket-server : unix-socket-client disconnected [" << m_settings.socketFileName << "]" << endl;
        constexpr int waitForSec = 1;
        VS_LOG_ERROR << "wait for [" << waitForSec << "] sec" << endl;
        std::this_thread::sleep_for( std::chrono::seconds(waitForSec) );

        // close client socket
        ::shutdown( m_clientSocketDscr, SHUT_RDWR );
        ::close( m_clientSocketDscr );
        m_clientSocketDscr = 0;
    }
}

std::string Shell::makeBlockedRequest( string _msg ){

    assert( Shell::EShellMode::CLIENT == m_settings.shellMode );

    // write to server
    const int writedBytesCount = send( m_clientSocketDscr, _msg.c_str(), _msg.size(), 0 );
    if( writedBytesCount != (int)_msg.size() ){

        // check for error
        if( writedBytesCount > 0 ){
            VS_LOG_ERROR << "unix-socket-client partial write [" << m_settings.socketFileName << "]" << endl;
        }
        else {
            VS_LOG_ERROR << "unix-socket-client write error [" << m_settings.socketFileName << "]" << endl;
        }
    }

    // read response
    char buf[ BYTES_COUNT_READ_FROM_SOCKET ];
    int readedBytesCount = recv( m_clientSocketDscr, buf, sizeof(buf), 0 );
    const string fromServer( buf, readedBytesCount );

    return fromServer;
}

PEnvironmentRequest Shell::getRequestInstance(){

    PShellRequest request = std::make_shared<ShellRequest>();
    request->clientSocketDscr = m_clientSocketDscr;
    request->clientModeInitiative = true;
    request->interface = this;
    request->m_asyncRequest = m_settings.asyncClientModeRequests;

    return request;
}

bool Shell::initClient(){

    struct sockaddr_un addr;

    // socket
    if( (m_clientSocketDscr = socket(AF_UNIX, SOCK_STREAM, 0)) == -1 ){
        VS_LOG_ERROR << "unix-socket-client socket create failed [" << m_settings.socketFileName << "]"
                  << " Reason [" << strerror( errno ) << "]"
                  << endl;
        return false;
    }

    // connect
    memset( & addr, 0, sizeof(addr) );
    addr.sun_family = AF_UNIX;
    strncpy( addr.sun_path, m_settings.socketFileName.c_str(), sizeof(addr.sun_path)-1 );

    // TODO: implement reconnect N times before to return a fail

    if( connect(m_clientSocketDscr, (struct sockaddr * ) & addr, sizeof(addr)) == -1 ){
        VS_LOG_ERROR << "unix-socket-client connect to unix-socket-server failed [" << m_settings.socketFileName << "]"
                  << " Reason [" << strerror( errno ) << "]"
                  << endl;
        return false;
    }

    VS_LOG_INFO << "unix-socket inited as client, path: " << m_settings.socketFileName
             << " max read bytes from socket: " << BYTES_COUNT_READ_FROM_SOCKET
             << endl;

    m_connectionEstablished = true;
    return true;
}

bool Shell::initServer(){

    struct sockaddr_un addr;    

    // socket
    if( (m_serverSocketDscr = socket(AF_UNIX, SOCK_STREAM, 0)) == -1 ){
        VS_LOG_ERROR << "unix-socket-server socket create failed [" << m_settings.socketFileName << "]"
                  << " Reason [" << strerror( errno ) << "]"
                  << endl;
        return false;
    }

    // unlink (?)
    memset( & addr, 0, sizeof(addr) );
    addr.sun_family = AF_UNIX;
    strncpy( addr.sun_path, m_settings.socketFileName.c_str(), sizeof(addr.sun_path)-1 );
    unlink( m_settings.socketFileName.c_str() );

    // bind
    if( bind(m_serverSocketDscr, (struct sockaddr * ) & addr, sizeof(addr)) == -1 ){
        VS_LOG_ERROR << "unix-socket-server bind failed, path: " << m_settings.socketFileName  << " (HINT: may be previous session was under ROOT user)"
                  << " Reason [" << strerror( errno ) << "]"
                  << endl;
        return false;
    }

    // listen
    if( listen(m_serverSocketDscr, 5) == -1 ){
        VS_LOG_ERROR << "unix-socket-server listen failed [" << m_settings.socketFileName << "]"
                  << " Reason [" << strerror( errno ) << "]"
                  << endl;
        return false;
    }

    VS_LOG_INFO << "unix-socket-server started, socket file: " << m_settings.socketFileName
             << " poll timeout millisec: " << m_settings.serverPollTimeoutMillisec
             << endl;

    m_connectionEstablished = true;
    return true;
}




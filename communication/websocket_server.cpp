
#ifndef Astra

#include "websocket_server.h"
#include "system/logger.h"

using namespace std;

static constexpr const char * PRINT_HEADER = "WebsocketServ:";

// NOTE: watch for buffer overflow
static constexpr int OUTCOMING_BUFFER_SIZE = 4096;

#define ENABLE_DEBUG_PRINTS 0

// -----------------------------------------------------------------------------
// request override
// -----------------------------------------------------------------------------
class WebsocketServerRequest : public AEnvironmentRequest {

public:
    WebsocketServerRequest()        
    {}

    virtual void setOutcomingMessage( const std::string & _msg ) override {

        if( ! websocketService->m_connectionEstablished.load() ){
            return;
        }

        SNetworkPackage package;
        package.header = m_header;
        package.msg = _msg;        
        websocketService->sendAsyncRequest( package, connectHandler );
    }

    virtual void * getUserData() override {
        // TODO: return conn id
    }

    SNetworkPackage::SHeader m_header;
    WebsocketServer * websocketService;
    websocketpp::connection_hdl connectHandler;
};
using PWebsocketServerRequest = std::shared_ptr<WebsocketServerRequest>;


// -----------------------------------------------------------------------------
// server
// -----------------------------------------------------------------------------
void callbackIncomingMessage( WebsocketServer * _userData,
                              websocketpp::connection_hdl _handler,
                              TWebsocketppServer::message_ptr _message ){

    TWebsocketppServer::connection_ptr wsConn = _userData->m_websocketServer.get_con_from_hdl( _handler );
    auto iter = _userData->m_connectionsIds.find( wsConn->get_uri()->get_host_port() );
    if( iter != _userData->m_connectionsIds.end() ){
        VS_LOG_DBG << "handler found in collection: host port " << iter->first
                   << " id " << iter->second
                   << endl;
    }

    switch( _message->get_opcode() ){
    case websocketpp::frame::opcode::value::binary : {

        SNetworkPackage * package = (SNetworkPackage *)_message->get_payload().data();

        PWebsocketServerRequest request = std::make_shared<WebsocketServerRequest>();
        request->m_incomingMessage.assign( ((char *)_message->get_payload().data()) + sizeof(SNetworkPackage::SHeader), _message->get_payload().size() - 1 );
        request->websocketService = _userData;
        request->m_header = package->header;
        request->connectHandler = _handler;
        request->m_connectionId = iter->second;

        if( request->m_incomingMessage.find("ping") == string::npos ){
            VS_LOG_INFO << "===========>"                        
                        // << " " << _handler.lock().get()
                        << " ws port [" << _userData->m_settings.port << "]"
                        << " bin msg from client [" << request->m_incomingMessage << "]"
                        << " flag 'from client' [" << request->m_header.m_clientInitiative << "]"
                        << endl;
            VS_LOG_INFO << "<==========="
                        << endl;
        }

        for( INetworkObserver * observer : _userData->m_observers ){
            observer->callbackNetworkRequest( request );
        }
        break;
    }
    case websocketpp::frame::opcode::value::text : {

        VS_LOG_INFO << _handler.lock().get()
                 << " text message from client: " << _message->get_payload()
                 << endl;

        PWebsocketServerRequest request = std::make_shared<WebsocketServerRequest>();
        request->m_incomingMessage = _message->get_payload();
        request->websocketService = _userData;

        for( INetworkObserver * observer : _userData->m_observers ){
            observer->callbackNetworkRequest( request );
        }
        break;
    }
    default : {
        VS_LOG_WARN << "unknown websocket opcode: " << _message->get_opcode()
                 << endl;
    }
    }

}

void callbackOnOpenHandle( WebsocketServer * _userData,
                           websocketpp::connection_hdl _handler ){    

    TWebsocketppServer::connection_ptr wsConn = _userData->m_websocketServer.get_con_from_hdl( _handler );
    _userData->m_connectionsIds.insert( {wsConn->get_uri()->get_host_port(), _userData->m_settings.generateConnectionId()} );
    _userData->m_connectionEstablished.store( true );
    _userData->m_handleOfFirstConnection = _handler;

    for( INetworkObserver * observer : _userData->m_observers ){
        observer->callbackConnectionEstablished( true, _userData->getConnId() );
    }
}

void callbackOnCloseHandle( WebsocketServer * _userData,
                           websocketpp::connection_hdl _handler ){

    VS_LOG_WARN << "websocket handler closed: " << _handler.lock().get() << endl;
    _userData->m_connectionEstablished.store( false );

    for( INetworkObserver * observer : _userData->m_observers ){
        observer->callbackConnectionEstablished( false, _userData->getConnId() );
    }
}

void callbackOnFail( WebsocketServer * _userData, websocketpp::connection_hdl _handler ){

    VS_LOG_WARN << "websocket fail on handler: " << _handler.lock().get() << endl;
    _userData->m_connectionEstablished.store( false );

    for( INetworkObserver * observer : _userData->m_observers ){
        observer->callbackConnectionEstablished( false, _userData->getConnId() );
    }
}

WebsocketServer::WebsocketServer( INetworkEntity::TConnectionId _id )
    : INetworkProvider(_id)
    , INetworkClient(_id)
    , m_connectionEstablished(false)
    , m_connectionIdGenerator(0)
{
    m_outcomingBuffer = new char[ OUTCOMING_BUFFER_SIZE ];
    m_outcomingBufferSize = OUTCOMING_BUFFER_SIZE;
}

WebsocketServer::~WebsocketServer()
{
    delete[] m_outcomingBuffer;
}


bool WebsocketServer::init( SInitSettings _settings ){

    m_settings = _settings;

    // set logging settings
    try{
        m_websocketServer.set_access_channels( websocketpp::log::alevel::all );
        m_websocketServer.clear_access_channels( websocketpp::log::alevel::frame_header );
        m_websocketServer.clear_access_channels( websocketpp::log::alevel::frame_payload );

        // initialize Asio
        m_websocketServer.init_asio();

        // register message handler
        m_websocketServer.set_message_handler( websocketpp::lib::bind( & callbackIncomingMessage,
                                                                       this,
                                                                       websocketpp::lib::placeholders::_1,
                                                                       websocketpp::lib::placeholders::_2 ) );

        m_websocketServer.set_open_handler( websocketpp::lib::bind( & callbackOnOpenHandle,
                                                                       this,
                                                                       websocketpp::lib::placeholders::_1 ) );

        m_websocketServer.set_close_handler( websocketpp::lib::bind( & callbackOnCloseHandle,
                                                                       this,
                                                                       websocketpp::lib::placeholders::_1 ) );

        m_websocketServer.set_fail_handler( websocketpp::lib::bind( & callbackOnFail,
                                                                       this,
                                                                       websocketpp::lib::placeholders::_1 ) );

        m_websocketServer.set_reuse_addr( true ); // NOTE: иначе после мгновенного перезапуска сокет еще занят
        m_websocketServer.listen( _settings.port );
        m_websocketServer.start_accept();

        VS_LOG_INFO << "WebsocketServer started at port: " << _settings.port << endl;
        return true;

    } catch( websocketpp::exception & _ex ){
        VS_LOG_ERROR << "WebsocketServer fail: " << _ex.what() << endl;
        return false;
    }
}

void WebsocketServer::addObserver( INetworkObserver * _observer ){

    m_observers.push_back( _observer );
}

void WebsocketServer::runNetworkCallbacks(){

    // TODO: m_settings.serverPollTimeoutMillisec ?
    m_websocketServer.poll();
}

void WebsocketServer::setPollTimeout( int32_t _timeoutMillsec ){
    m_settings.serverPollTimeoutMillisec = _timeoutMillsec;
}

void WebsocketServer::shutdown(){

    // TODO: ?
}

PEnvironmentRequest WebsocketServer::getRequestInstance(){

    std::lock_guard<std::mutex> lock( m_mutexSendProtection );
    PWebsocketServerRequest request = std::make_shared<WebsocketServerRequest>();
    request->websocketService = this;
    request->connectHandler = m_handleOfFirstConnection;
    return request;
}

void WebsocketServer::sendAsyncRequest( const SNetworkPackage & _package, websocketpp::connection_hdl & _connectHandle ){

    std::lock_guard<std::mutex> lock( m_mutexSendProtection );

    // dynamic growing
    if( _package.msg.size() > (m_outcomingBufferSize + 1) ){
        delete[] m_outcomingBuffer;
        m_outcomingBuffer = new char[ m_outcomingBufferSize * 2 ];
        m_outcomingBufferSize *= 2;
    }

    //
    memcpy( m_outcomingBuffer, & _package.header, sizeof(SNetworkPackage::SHeader) );
    char * messageSection = m_outcomingBuffer + sizeof(SNetworkPackage::SHeader);
    memcpy( messageSection, _package.msg.data(), _package.msg.size() );

#if ENABLE_DEBUG_PRINTS
    if( _package.msg.find("\"message\":\"pong\"") == string::npos ){
        LOG_DBG << PRINT_HEADER_TRACE << " send msg [" << _package.msg << "]" << endl;
    }
#endif

    //
    try{
        m_websocketServer.send( _connectHandle,
                                m_outcomingBuffer,
                                sizeof(SNetworkPackage::SHeader) + _package.msg.size(),
                                websocketpp::frame::opcode::value::binary );
    } catch( websocketpp::exception & _ex ){
        VS_LOG_ERROR << "ws exception: " << _ex.what() << endl;
        return;
    }
}


#endif

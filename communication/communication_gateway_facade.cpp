
// available transport mechanisms
#include "webserver.h"
#include "http_client.h"
#include "amqp_client_c.h"
#include "shell.h"
#include "websocket_server.h"
#include "shared_memory_server.h"
#include "objrepr_listener.h"

#include "system/logger.h"
#include "system/a_config_reader.h"
#include "common/ms_common_utils.h"
#include "communication_gateway_facade.h"

using namespace std;

static constexpr const char * PRINT_HEADER = "CommunicationGateway:";

CommunicationGatewayFacade::CommunicationGatewayFacade()
    : m_connectionIdGenerator(0)
    , m_shutdownCalled(false)
    , m_threadNetworkCallbacks(nullptr)
{

}

CommunicationGatewayFacade::~CommunicationGatewayFacade()
{
    if( ! m_shutdownCalled ){
        shutdown();
    }        
}

void CommunicationGatewayFacade::callbackNetworkRequest( PEnvironmentRequest _request ){

    // request can income from anywere: Network, Shell, Config, WAL, etc...

    m_mutexCommand.lock();
    PCommand command = m_settings.specParams.factory->createCommand(_request);
    if( command ){
        m_arrivedCommands.push( command );
    }
    m_mutexCommand.unlock();
}

bool CommunicationGatewayFacade::isCommandAvailable(){

    bool out;
    m_mutexCommand.lock();
    out = ! m_arrivedCommands.empty();
    m_mutexCommand.unlock();
    return out;
}

PCommand CommunicationGatewayFacade::nextIncomingCommand(){

    m_mutexCommand.lock();
    PCommand cmd = m_arrivedCommands.front();
    m_arrivedCommands.pop();
    m_mutexCommand.unlock();

    return cmd;
}


bool CommunicationGatewayFacade::init( const SInitSettings & _settings ){

    m_settings = _settings;

    // TODO: watch for requests intersection in Config & WAL

    // commands from WAL
    for( PEnvironmentRequest & request : _settings.requestsFromWAL ){
        callbackNetworkRequest( request );
    }
    // commands from config
    for( PEnvironmentRequest & request : _settings.requestsFromConfig ){
        callbackNetworkRequest( request );
    }

    //
    if( ! initialConnections(_settings) ){
        return false;
    }

    //
    if( ! initDerive(_settings) ){
        return false;
    }

    // define poll timeout
    int32_t pollTimeoutPerEachNetworkMillisec = _settings.pollTimeoutMillisec / m_externalNetworks.size();
    for( PNetworkProvider & net : m_externalNetworks ){
        net->setPollTimeout( pollTimeoutPerEachNetworkMillisec );
    }

    //
    if( _settings.asyncNetwork ){
        m_threadNetworkCallbacks = new std::thread( & CommunicationGatewayFacade::threadNetworkCallbacks, this );
    }


    VS_LOG_INFO << PRINT_HEADER << " init success" << endl;
    return true;
}

void CommunicationGatewayFacade::shutdown(){

    VS_LOG_INFO << PRINT_HEADER << " begin shutdown" << endl;

    m_shutdownCalled = true;
    common_utils::threadShutdown( m_threadNetworkCallbacks );
    m_externalNetworks.clear();
    m_internalNetworks.clear();

    VS_LOG_INFO << PRINT_HEADER << " shutdown success" << endl;
}

#ifdef OBJREPR_LIBRARY_EXIST
PObjreprListener g_objreprListener;
#endif

bool CommunicationGatewayFacade::initialConnections( const SInitSettings & _settings ){

    // shell ( domain socket )
    if( _settings.paramsForInitialShell.enable ){
        Shell::SInitSettings settings1;
        settings1.shellMode = ( _settings.paramsForInitialShell.client ? Shell::EShellMode::CLIENT : Shell::EShellMode::SERVER );
        settings1.asyncServerMode = _settings.paramsForInitialShell.asyncServerMode;
        settings1.asyncClientModeRequests = _settings.paramsForInitialShell.asyncClientModeRequests;
        settings1.socketFileName = _settings.paramsForInitialShell.socketName;
        settings1.messageMode = ( _settings.paramsForInitialShell.withSizeHeader
                                  ? Shell::EMessageMode::WITH_SIZE
                                  : Shell::EMessageMode::WITHOUT_SIZE );

        PShell shell = std::make_shared<Shell>( getConnectionId() );
        if( ! shell->init(settings1) ){
            return false;
        }

        shell->addObserver( this );
        m_externalNetworks.push_back( shell );
    }

    // webserver
    if( _settings.paramsForInitialHTTPServer.enable ){
        WebServer::SInitSettings settings2;
        settings2.listenPort = _settings.paramsForInitialHTTPServer.port;
        settings2.async = false;

        PWebserver webserver = std::make_shared<WebServer>( getConnectionId() );
        if( ! webserver->init(settings2) ){
            return false;
        }

        webserver->addObserver( this );
        m_externalNetworks.push_back( webserver );
    }

    // amqp ( as external transport )
    if( _settings.paramsForInitialAmqp.enable ){
        // amqp client
        AmqpClient::SInitSettings settings;
        settings.asyncMode = false;
        settings.serverHost = _settings.paramsForInitialAmqp.host;
        settings.port = _settings.paramsForInitialAmqp.port;
        settings.login = _settings.paramsForInitialAmqp.login;
        settings.pass = _settings.paramsForInitialAmqp.pass;
        settings.amqpVirtualHost = _settings.paramsForInitialAmqp.virtHost;

        PAmqpClient amqpClient = std::make_shared<AmqpClient>( getConnectionId() );
        if( ! amqpClient->init( settings ) ){
            VS_LOG_ERROR << "network amqp-client init fail: " << amqpClient->getState().m_lastError << endl;
            return false;
        }

        amqpClient->addObserver( this );
        m_externalNetworks.push_back( amqpClient );
        m_initialAmqpClient = amqpClient;

        // amqp controller ( with specialization )
        AmqpController::SInitSettings settings2;
        settings2.client = amqpClient;
        settings2.route = _settings.paramsForInitialAmqp.route;

        PAmqpController controller = std::make_shared<AmqpController>( amqpClient->getConnId() );
        const bool rt = controller->init( settings2 );
        if( ! rt ){
            VS_LOG_ERROR << PRINT_HEADER << " amqp-controller init fail: " << controller->getState().lastError << endl;
            return false;
        }
        m_clientNetworks.push_back( controller );
    }

    // websocket
    if( _settings.paramsForInitialWebsocketServer.enable ){
#ifndef Astra
        WebsocketServer::SInitSettings settings4;
        settings4.port = _settings.paramsForInitialWebsocketServer.port;
        settings4.async = false;
        settings4.generateConnectionId = std::bind( & CommunicationGatewayFacade::getConnectionId, this );

        PWebsocketServer websocketServer = std::make_shared<WebsocketServer>( getConnectionId() );
        if( ! websocketServer->init(settings4) ){
            return false;
        }

        websocketServer->addObserver( this );
        m_externalNetworks.push_back( websocketServer );
#else
        VS_LOG_WARN << "websocket server for Astra Linux is NOT yet supported" << endl;
#endif
    }

    // objrepr bus
    if( _settings.paramsForInitialObjrepr.enable ){
#ifdef OBJREPR_LIBRARY_EXIST
        ObjreprListener::SInitSettings settings;
        settings.listenedObjectId = _settings.paramsForInitialObjrepr.serverMirrorIdInContext;
        settings.withPackageHeader = false;

        PObjreprListener objreprListener = std::make_shared<ObjreprListener>( getConnectionId() );
        if( ! objreprListener->init(settings) ){
            return false;
        }

        objreprListener->addObserver( this );
        m_externalNetworks.push_back( objreprListener );
        g_objreprListener = objreprListener;
#endif
    }

    // TODO: shared memory


    return true;
}

PNetworkClient CommunicationGatewayFacade::getInitialAmqpConnection(){
    return m_initialAmqpClient;
}

PNetworkClient CommunicationGatewayFacade::getInitialShellConnection(){
    assert( false && "TODO: do" );
}

PNetworkClient CommunicationGatewayFacade::getInitialHTTPClientConnection(){
    assert( false && "TODO: do" );
}

PNetworkProvider CommunicationGatewayFacade::getInitialHTTPServerConnection(){
    assert( false && "TODO: do" );
}

PNetworkProvider CommunicationGatewayFacade::getInitialWebsocketServerConnection(){
    assert( false && "TODO: do" );
}

PNetworkClient CommunicationGatewayFacade::getInitialSharedMemConnection(){
    assert( false && "TODO: do" );
}

PNetworkClient CommunicationGatewayFacade::getInitialObjreprConnection(){
#ifdef OBJREPR_LIBRARY_EXIST
    return g_objreprListener;
#else
    return nullptr;
#endif
}

PNetworkClient CommunicationGatewayFacade::getNewAmqpConnection( const SConnectParamsAmqp & _params ){

    //


    return nullptr;
}

PNetworkClient CommunicationGatewayFacade::getNewShellConnection( const SConnectParamsShell & _params ){

    // shell ( domain socket )
    Shell::SInitSettings settings1;
    settings1.shellMode = ( _params.client ? Shell::EShellMode::CLIENT : Shell::EShellMode::SERVER );
    settings1.asyncServerMode = _params.asyncServerMode;
    settings1.asyncClientModeRequests = _params.asyncClientModeRequests;
    settings1.socketFileName = _params.socketName;
    settings1.messageMode = ( _params.withSizeHeader
                              ? Shell::EMessageMode::WITH_SIZE
                              : Shell::EMessageMode::WITHOUT_SIZE );

    PShell shell = std::make_shared<Shell>( getConnectionId() );
    if( ! shell->init(settings1) ){
        return nullptr;
    }

    return shell;
}

PNetworkClient CommunicationGatewayFacade::getNewHTTPClientConnection( const SConnectParamsHTTPClient & _params ){

    PHTTPClient httpClient = std::make_shared<HTTPClient>( getConnectionId() );

    m_clientNetworks.push_back( httpClient );

    return httpClient;
}

PNetworkProvider CommunicationGatewayFacade::getNewHTTPServerConnection( const SConnectParamsHTTPServer & _params ){

    WebServer::SInitSettings settings2;
    settings2.listenPort = _params.port;
    settings2.async = false;

    PWebserver webserver = std::make_shared<WebServer>( getConnectionId() );
    if( ! webserver->init(settings2) ){
        return nullptr;
    }

    webserver->addObserver( this );
    m_externalNetworks.push_back( webserver );

    return webserver;
}

PNetworkProvider CommunicationGatewayFacade::getNewWebsocketServerConnection( const SConnectParamsWebsocketServer & _params ){

    WebsocketServer::SInitSettings settings4;
    settings4.port = _params.port;
    settings4.async = false;
    settings4.generateConnectionId = std::bind( & CommunicationGatewayFacade::getConnectionId, this );

    PWebsocketServer websocketServer = std::make_shared<WebsocketServer>( getConnectionId() );
    if( ! websocketServer->init(settings4) ){
        return nullptr;
    }

    websocketServer->addObserver( this );
    m_externalNetworks.push_back( websocketServer );

    return websocketServer;
}

PNetworkClient CommunicationGatewayFacade::getNewSharedMemConnection( const SConnectParamsSharedMem & _params ){

    // TODO: do

    return nullptr;
}

PNetworkEntity CommunicationGatewayFacade::getConnection( INetworkEntity::TConnectionId _connId ){

    auto iter1 = m_internalNetworksById.find( _connId );
    if( iter1 != m_internalNetworksById.end() ){
        return iter1->second;
    }
    else{
        auto iter2 = m_externalNetworksById.find( _connId );
        if( iter2 != m_externalNetworksById.end() ){
            return iter2->second;
        }
        else{
            VS_LOG_ERROR << PRINT_HEADER << " no available network client with id [" << _connId << "]" << endl;
            return nullptr;
        }
    }
}

PNetworkClient CommunicationGatewayFacade::getFileDownloader(){

    // TODO: do
    PHTTPClient httpClient = std::make_shared<HTTPClient>( getConnectionId() );

    m_clientNetworks.push_back( httpClient );

    return httpClient;
}

void CommunicationGatewayFacade::runNetworkCallbacks(){

    assert( ! m_settings.asyncNetwork && "network callbacks shouldn't be async in this case" );

    for( PNetworkProvider & net : m_externalNetworks ){
        net->runNetworkCallbacks();
    }

    for( PNetworkProvider & net : m_internalNetworks ){
        net->runNetworkCallbacks();
    }
}

void CommunicationGatewayFacade::threadNetworkCallbacks(){

    VS_LOG_INFO << PRINT_HEADER << " async network callbacks THREAD started" << endl;

    // TODO: protect at run-time internal networks client adding
    while( ! m_shutdownCalled ){

        // external links
        for( PNetworkProvider & net : m_externalNetworks ){
            net->runNetworkCallbacks();
        }

        // internal links
        for( auto iter = m_internalNetworks.begin(); iter != m_internalNetworks.end(); ){
            PNetworkProvider & net = ( * iter );

            if( ! net->isConnectionEstablished() && 1 == net.use_count() ){
                VS_LOG_INFO << PRINT_HEADER << " ========== internal network has no connection & no longer used, destroy it" << endl;
                iter = m_internalNetworks.erase( iter );
            }
            else{
                ++iter;
            }
        }

        // TODO: uncomment
//        for( PNetworkProvider & net : m_internalNetworks ){
//            net->runNetworkCallbacks();
//        }

        this_thread::sleep_for( chrono::milliseconds(100) );
    }

    VS_LOG_INFO << PRINT_HEADER << " async network callbacks THREAD stopped" << endl;
}

INetworkEntity::TConnectionId CommunicationGatewayFacade::getConnectionId(){
    return m_connectionIdGenerator++;
}









#include <iostream>

#include "3rd_party/mongoose.h"

#include "common/ms_common_utils.h"
#include "system/logger.h"
#include "unified_command_convertor.h"
#include "webserver.h"

using namespace std;

static struct mg_serve_http_opts g_httpServerOptions;

// -----------------------------------------------------------------------------
// request override
// -----------------------------------------------------------------------------
class WebserverRequest : public AEnvironmentRequest {

public:
    virtual void setOutcomingMessage( const std::string & _msg ) override {

        mg_send_head( mongooseConnection, httpCode, _msg.size(), "Content-Type: application/json" );
        mg_printf( mongooseConnection, "%.*s", (int)_msg.size(), _msg.data() );

        VS_LOG_INFO << "http server send body [" << _msg << "]" << endl;
    }

    virtual void setOutcomingMessage( const char * _bytes, int _bytesLen ) override {

        // TODO: base64
    }

    virtual void setUserData( void * _data ) override {
        httpCode = ( * (int *)_data );
    }

    mg_connection * mongooseConnection;
    std::string incomingQueryString;
    int httpCode;
};
using WebserverRequestPtr = std::shared_ptr<WebserverRequest>;

// -----------------------------------------------------------------------------
// private implementation
// -----------------------------------------------------------------------------
struct SWebServerPrivateImplementation {

    SWebServerPrivateImplementation() :
        serverThreadRun(false),
        threadWebServerListen(nullptr),
        mongooseEventManager(nullptr){}
    bool                                serverThreadRun;
    std::thread *                       threadWebServerListen;
    mg_mgr *                            mongooseEventManager;
    int                                 pollTimeoutMillisec;
    std::vector< INetworkObserver * > observers;
    UnifiedCommandConvertor * commandConvertor;
};

// -----------------------------------------------------------------------------
// mongoose handler
// -----------------------------------------------------------------------------
void callbackEventHandler2( mg_connection * _connect, int _eventType, void * _eventData ){

    switch( _eventType ){
    case MG_EV_HTTP_REQUEST : {

        WebServer * webserver = ( WebServer * )_connect->user_data;

        // data from client
        http_message * hm = (http_message *)_eventData;

        // prepare request
        WebserverRequestPtr request = make_shared<WebserverRequest>();
        const string method( (char *)hm->method.p, hm->method.len );
        const string uri( (char *)hm->uri.p, hm->uri.len );
        request->m_incomingMessage.assign( (char *)hm->body.p, hm->body.len );
        request->incomingQueryString.assign( (char *)hm->query_string.p, hm->query_string.len );
        request->mongooseConnection = _connect;
        request->m_connectionId = webserver->getConnId();

//        VS_LOG_INFO << "webserver:"
//                    << " uri [" << uri << "]"
//                    << " method [" << method << "]"
//                    << " GET [" << request->incomingQueryString << "]"
//                    << " POST body size [" << request->m_incomingMessage.size() << "]"
////                    << " POST body [" << request->m_incomingMessage << "]"
//                    << endl;

        // convert command from HTTP format to internal representation
        request->m_incomingMessage = webserver->m_impl->commandConvertor->getCommandsFromHTTPRequest( method, uri, request->incomingQueryString, request->m_incomingMessage );

        // notify observers
        for( INetworkObserver * observer : webserver->m_impl->observers ){
            observer->callbackNetworkRequest( request );
        }

        break;
    }
    case MG_EV_WEBSOCKET_HANDSHAKE_REQUEST : {
        VS_LOG_TRACE << "MG_EV_WEBSOCKET_HANDSHAKE_REQUEST" << endl;
        break;
    }
    case MG_EV_WEBSOCKET_HANDSHAKE_DONE : {
        VS_LOG_TRACE << "MG_EV_WEBSOCKET_HANDSHAKE_DONE" << endl;
        break;
    }
    case MG_EV_WEBSOCKET_FRAME : {
        VS_LOG_TRACE << "MG_EV_WEBSOCKET_FRAME" << endl;
        break;
    }
    case MG_EV_WEBSOCKET_CONTROL_FRAME : {
        VS_LOG_TRACE << "MG_EV_WEBSOCKET_CONTROL_FRAME" << endl;
        break;
    }
    default : break;
    }
}

// -----------------------------------------------------------------------------
// webserver
// -----------------------------------------------------------------------------
WebServer::WebServer( INetworkEntity::TConnectionId _id )
    : INetworkProvider(_id)
    , m_impl(new SWebServerPrivateImplementation)
{


}

WebServer::~WebServer(){

    shutdown();
}

bool WebServer::init( SInitSettings _settings ){

    m_impl->pollTimeoutMillisec = _settings.pollTimeoutMillisec;

    // mongoose - 1. init event manager
    m_impl->mongooseEventManager = new mg_mgr();
    m_impl->commandConvertor = _settings.commandConvertor;
    mg_mgr_init( m_impl->mongooseEventManager, nullptr );

    // mongoose - 1.1 set HTTP server options
    mg_bind_opts bindOptions;
    memset( & bindOptions, 0, sizeof(bindOptions) );
    const char * err_str;
    bindOptions.error_string = & err_str;

    // mongoose - 2. create connection
    mg_connection * mongooseConnect = nullptr;
    mongooseConnect = mg_bind_opt( m_impl->mongooseEventManager, to_string(_settings.listenPort).c_str(), callbackEventHandler2, bindOptions );
    if( ! mongooseConnect ){
        VS_LOG_ERROR << "error starting webserver on port {1} : {2} " << _settings.listenPort << " " << * bindOptions.error_string << endl;
        return false;
    }
    mongooseConnect->user_data = this;

    // mongoose - 3. http protocol
    mg_set_protocol_http_websocket( mongooseConnect );

    // mode
    if( _settings.async ){
        m_impl->serverThreadRun = true;
        m_impl->threadWebServerListen = new std::thread( & WebServer::threadWebServerListen, this );
    }        

    VS_LOG_INFO << "WebServer started, port: [1], poll timeout millisec: [2] " << _settings.listenPort << " " << m_impl->pollTimeoutMillisec << endl;

    return true;
}

void WebServer::shutdown(){

    m_impl->serverThreadRun = false;

    common_utils::threadShutdown( m_impl->threadWebServerListen );

    mg_mgr_free( m_impl->mongooseEventManager );

    delete m_impl;
    m_impl = nullptr;
}

void WebServer::addObserver( INetworkObserver * _observer ){
    m_impl->observers.push_back( _observer );
}

void WebServer::runNetworkCallbacks(){
    mg_mgr_poll( m_impl->mongooseEventManager, m_impl->pollTimeoutMillisec );
}

void WebServer::setPollTimeout( int32_t _timeoutMillsec ){
    m_impl->pollTimeoutMillisec = _timeoutMillsec;
}

void WebServer::threadWebServerListen(){

    while( m_impl->serverThreadRun ){
        runNetworkCallbacks();
    }
}

// -----------------------------------------------------------------------------
// test
// -----------------------------------------------------------------------------
static void handle_sum_call(struct mg_connection *nc, struct http_message *hm) {
    char n1[100], n2[100];
    double result;

    /* Get form variables */
    int n1s = mg_get_http_var(&hm->body, "n1", n1, sizeof(n1));
    int n2s = mg_get_http_var(&hm->body, "n2", n2, sizeof(n2));

    //  hm->query_string

    /* Send headers */
    mg_printf(nc, "%s", "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n");

    /* Compute the result and send it back as a JSON object */
    result = strtod(n1, NULL) + strtod(n2, NULL);
    mg_printf_http_chunk(nc, "{ \"result\": %lf }", result);
    mg_send_http_chunk(nc, "blabla", 6);
    mg_send_http_chunk(nc, "", 0); /* Send empty chunk, the end of response */
}

static void callbackEventHandler( mg_connection * _connect, int _eventType, void * _eventData ){

    // data
    http_message * hm = (http_message *)_eventData;

    // to send response
    const string msg = "blabla";


//    ((WebServer *)(_connect->user_data))->sendResponse( msg );

    switch( _eventType ){
    case MG_EV_HTTP_REQUEST:

        if( mg_vcmp( & hm->uri, "/api/v1/sum") == 0 ){

            handle_sum_call(_connect, hm); /* Handle RESTful call */
        }
        else if( mg_vcmp( & hm->uri, "/printcontent") == 0 ){

            char buf[100] = {0};
            memcpy( buf, hm->body.p, sizeof(buf) - 1 < hm->body.len ? sizeof(buf) - 1 : hm->body.len );
            printf("%s\n", buf);

        }
        else {

            mg_serve_http( _connect, hm, g_httpServerOptions ); /* Serve static content */
        }
        break;
    case MG_EV_WEBSOCKET_HANDSHAKE_REQUEST : {
        VS_LOG_TRACE << "MG_EV_WEBSOCKET_HANDSHAKE_REQUEST" << endl;
        break;
    }
    case MG_EV_WEBSOCKET_HANDSHAKE_DONE : {
        VS_LOG_TRACE << "MG_EV_WEBSOCKET_HANDSHAKE_DONE" << endl;
        break;
    }
    case MG_EV_WEBSOCKET_FRAME : {
        VS_LOG_TRACE << "MG_EV_WEBSOCKET_FRAME" << endl;

        struct websocket_message *wm = (struct websocket_message *) _eventData;

        VS_LOG_TRACE << wm->data << endl;

        string s = "message_from_websocket_server";
        mg_send_websocket_frame(  _connect, WEBSOCKET_OP_TEXT, s.c_str(), s.size() );

        break;
    }
    case MG_EV_WEBSOCKET_CONTROL_FRAME : {
        VS_LOG_TRACE << "MG_EV_WEBSOCKET_CONTROL_FRAME" << endl;
        break;
    }
    default:
        break;
    }
}








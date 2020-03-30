
#include <sstream>
#include <cstring>
#include <pthread.h>

#include <boost/format.hpp>

#include "system/logger.h"
#include "common/ms_common_utils.h"
#include "amqp_client_c.h"

using namespace std;

static constexpr const char * PRINT_HEADER = "AmqpClient:";
static constexpr std::array<const char *, 3> g_exchangePointType = { "direct", "fanout", "topic" };

static inline string amqpStatusStr( int _status ){

    switch( _status ){
    case amqp_status_enum_::AMQP_STATUS_OK : { return "AMQP_STATUS_OK"; }
    case amqp_status_enum_::AMQP_STATUS_NO_MEMORY: { return "AMQP_STATUS_NO_MEMORY"; }
    case amqp_status_enum_::AMQP_STATUS_BAD_AMQP_DATA: { return "AMQP_STATUS_BAD_AMQP_DATA"; }
    case amqp_status_enum_::AMQP_STATUS_UNKNOWN_CLASS: { return "AMQP_STATUS_UNKNOWN_CLASS"; }
    case amqp_status_enum_::AMQP_STATUS_UNKNOWN_METHOD : { return "AMQP_STATUS_UNKNOWN_METHOD"; }
    case amqp_status_enum_::AMQP_STATUS_HOSTNAME_RESOLUTION_FAILED: { return "AMQP_STATUS_HOSTNAME_RESOLUTION_FAILED"; }
    case amqp_status_enum_::AMQP_STATUS_INCOMPATIBLE_AMQP_VERSION : { return "AMQP_STATUS_INCOMPATIBLE_AMQP_VERSION"; }
    case amqp_status_enum_::AMQP_STATUS_CONNECTION_CLOSED : { return "AMQP_STATUS_CONNECTION_CLOSED"; }
    case amqp_status_enum_::AMQP_STATUS_BAD_URL : { return "AMQP_STATUS_BAD_URL"; }
    case amqp_status_enum_::AMQP_STATUS_SOCKET_ERROR : { return "AMQP_STATUS_SOCKET_ERROR"; }
    case amqp_status_enum_::AMQP_STATUS_INVALID_PARAMETER : { return "AMQP_STATUS_INVALID_PARAMETER"; }
    case amqp_status_enum_::AMQP_STATUS_TABLE_TOO_BIG : { return "AMQP_STATUS_TABLE_TOO_BIG"; }
    case amqp_status_enum_::AMQP_STATUS_WRONG_METHOD : { return "AMQP_STATUS_WRONG_METHOD"; }
    case amqp_status_enum_::AMQP_STATUS_TIMEOUT : { return "AMQP_STATUS_TIMEOUT"; }
    case amqp_status_enum_::AMQP_STATUS_TIMER_FAILURE : { return "AMQP_STATUS_TIMER_FAILURE"; }
    case amqp_status_enum_::AMQP_STATUS_HEARTBEAT_TIMEOUT : { return "AMQP_STATUS_HEARTBEAT_TIMEOUT"; }
    case amqp_status_enum_::AMQP_STATUS_UNEXPECTED_STATE : { return "AMQP_STATUS_UNEXPECTED_STATE"; }
#ifndef Astra
    case amqp_status_enum_::AMQP_STATUS_SOCKET_CLOSED : { return "AMQP_STATUS_SOCKET_CLOSED"; }
    case amqp_status_enum_::AMQP_STATUS_SOCKET_INUSE : { return "AMQP_STATUS_SOCKET_INUSE"; }
    case amqp_status_enum_::AMQP_STATUS_UNSUPPORTED : { return "AMQP_STATUS_UNSUPPORTED"; }
    case amqp_status_enum_::AMQP_STATUS_BROKER_UNSUPPORTED_SASL_METHOD : { return "AMQP_STATUS_BROKER_UNSUPPORTED_SASL_METHOD"; }
    case amqp_status_enum_::_AMQP_STATUS_TCP_NEXT_VALUE : { return "_AMQP_STATUS_TCP_NEXT_VALUE"; }
    case amqp_status_enum_::_AMQP_STATUS_SSL_NEXT_VALUE : { return "_AMQP_STATUS_SSL_NEXT_VALUE"; }
    case amqp_status_enum_::_AMQP_STATUS_NEXT_VALUE : { return "_AMQP_STATUS_NEXT_VALUE"; }
#endif
    case amqp_status_enum_::AMQP_STATUS_TCP_ERROR : { return "AMQP_STATUS_TCP_ERROR"; }
    case amqp_status_enum_::AMQP_STATUS_TCP_SOCKETLIB_INIT_ERROR : { return "AMQP_STATUS_TCP_SOCKETLIB_INIT_ERROR"; }
    case amqp_status_enum_::AMQP_STATUS_SSL_ERROR : { return "AMQP_STATUS_SSL_ERROR"; }
    case amqp_status_enum_::AMQP_STATUS_SSL_HOSTNAME_VERIFY_FAILED: { return "AMQP_STATUS_SSL_HOSTNAME_VERIFY_FAILED"; }
    case amqp_status_enum_::AMQP_STATUS_SSL_PEER_VERIFY_FAILED : { return "AMQP_STATUS_SSL_PEER_VERIFY_FAILED"; }
    case amqp_status_enum_::AMQP_STATUS_SSL_CONNECTION_FAILED : { return "AMQP_STATUS_SSL_CONNECTION_FAILED"; }
    default : { return "unexpected amqp status" + std::to_string(_status); }
    }
}

static inline string amqpStrError( const amqp_rpc_reply_t ret ){

    switch( ret.reply_type ){
    case AMQP_RESPONSE_NORMAL : { return string(); }
    case AMQP_RESPONSE_NONE : { return "missing RPC reply type"; }
    case AMQP_RESPONSE_LIBRARY_EXCEPTION : { return ::amqp_error_string2( ret.library_error ); }
    case AMQP_RESPONSE_SERVER_EXCEPTION : {
        switch( ret.reply.id ){
        case AMQP_CONNECTION_CLOSE_METHOD: {
            amqp_connection_close_t * m = (amqp_connection_close_t *) ret.reply.decoded;
            ostringstream out;
            out << "amqp server connection error " << m->reply_code << ", message: " << std::string((char *) m->reply_text.bytes, m->reply_text.len);
            return out.str();
        }
        case AMQP_CHANNEL_CLOSE_METHOD: {
            amqp_channel_close_t * m = (amqp_channel_close_t *) ret.reply.decoded;
            ostringstream out;
            out << "amqp server channel error " << m->reply_code << ", message: " << std::string((char *) m->reply_text.bytes, m->reply_text.len);
            return out.str();
        }
        default : {
            ostringstream out;
            out << "unknown amqp server error, method id: " << ret.reply.id;
            return out.str();
        }
        }
        break;
    }
    default : { return "unknown amqp error"; }
    }
}

// -----------------------------------------------------------------------------
// request override
// -----------------------------------------------------------------------------
class AmqpRequest : public AEnvironmentRequest {
public:
    AmqpRequest()
        : networkClient(nullptr)
        , routingTarget(nullptr)
    {}

    // async mode
    virtual std::string sendMessageAsync( const std::string & _msg, const std::string & _correlationId = "" ) override {

        // requester
        if( AEnvironmentRequest::m_correlationId.empty() ){
            AEnvironmentRequest::m_requestTimeMillisec = common_utils::getCurrentTimeMillisec();
            AEnvironmentRequest::m_correlationId = common_utils::generateUniqueId();
            const string replyTo = routingTarget->predatorExchangePointName + AmqpClient::REPLY_TO_DELIMETER + routingTarget->predatorRoutingKeyName;

            const bool rt = networkClient->sendPackageAsync( _msg,
                                                             AEnvironmentRequest::m_correlationId,
                                                             routingTarget->targetExchangePointName,
                                                             routingTarget->targetRoutingKeyName,
                                                             replyTo );
            return AEnvironmentRequest::m_correlationId;
        }
        // replier
        else{
            const string & corrId = m_correlationId;
            const string replyToDummy = "i_am_replier";
            const string ep = replyTo.substr( 0, replyTo.find(AmqpClient::REPLY_TO_DELIMETER) );
            const string rk = replyTo.substr( replyTo.find(AmqpClient::REPLY_TO_DELIMETER) + 1, replyTo.size() - replyTo.find(AmqpClient::REPLY_TO_DELIMETER) );

            const bool rt = networkClient->sendPackageAsync( _msg,
                                                             corrId,
                                                             ep,
                                                             rk,
                                                             replyToDummy );
            return corrId;
        }
    }

    virtual bool checkResponseReadyness() override {

        if( (common_utils::getCurrentTimeMillisec() - AEnvironmentRequest::m_requestTimeMillisec) > networkClient->m_state.settings.deliveredMessageExpirationSec * 1000 ){
            VS_LOG_WARN << PRINT_HEADER << " request timeouted, corr id [" << AEnvironmentRequest::m_correlationId << "]" << endl;
            AEnvironmentRequest::m_timeouted = true;
            networkClient->m_refusedMessages.insert( AEnvironmentRequest::m_correlationId );
            AEnvironmentRequest::m_correlationId.clear();
            return false;
        }

        return networkClient->checkResponseReadyness( AEnvironmentRequest::m_correlationId );
    }

    virtual std::string getAsyncResponse() override {
        const string out =  networkClient->getAsyncResponse( AEnvironmentRequest::m_correlationId );
        AEnvironmentRequest::m_correlationId.clear();
        return out;
    }

    // blocked mode
    virtual void setOutcomingMessage( const std::string & _msg ) override {
        const string corrId = common_utils::generateUniqueId();
        const string replyTo = routingTarget->predatorExchangePointName + AmqpClient::REPLY_TO_DELIMETER + routingTarget->predatorRoutingKeyName;

        AEnvironmentRequest::m_incomingMessage = networkClient->sendPackageBlocked( _msg,
                                                                                    corrId,
                                                                                    routingTarget->targetExchangePointName,
                                                                                    routingTarget->targetRoutingKeyName,
                                                                                    replyTo );
    }

    // user defined stuff
    virtual void setUserData( void * _data ) override {
        routingTarget = static_cast<SAmqpRouteParameters *>( _data );
    }

    AmqpClient * networkClient;
    SAmqpRouteParameters * routingTarget;
    std::string replyTo;
};
using PAmqpRequest = std::shared_ptr<AmqpRequest>;

// -----------------------------------------------------------------------------
// client
// -----------------------------------------------------------------------------
AmqpClient::AmqpClient( INetworkEntity::TConnectionId _id )
    : INetworkProvider(_id)
    , INetworkClient(_id)
    , m_shutdownCalled(false)
    , m_connTransmit(nullptr)
    , m_connReceive(nullptr)
    , m_threadIncomingPackages(nullptr)
    , m_syncRequestPerformed(false)
{

}

AmqpClient::~AmqpClient(){

    shutdown();
}

bool AmqpClient::init( const SInitSettings & _settings ){

    m_state.settings = _settings;
    m_msgExpirationMillisecStr = std::to_string( _settings.deliveredMessageExpirationSec * 1000 );

    // init amqp
    if( ! initLowLevel(m_connTransmit, _settings) ){
        return false;
    }

    if( ! initLowLevel(m_connReceive, _settings) ){
        return false;
    }

    // run thread
    if( _settings.asyncMode ){
        m_threadIncomingPackages = new std::thread( & AmqpClient::threadReceiveLoop, this );
    }

    const string msg = ( boost::format( "%1% connected to [%2%]:[%3%] with [%4%] / [%5%]. Virtual host [%6%]. Async mode [%7%] Async message expiration timeout in sec [%8%]" )
                         % PRINT_HEADER
                         % _settings.serverHost
                         % _settings.port
                         % _settings.login
                         % _settings.pass
                         % _settings.amqpVirtualHost
                         % (_settings.asyncMode ? "TRUE" : "FALSE")
                         % _settings.deliveredMessageExpirationSec
                       ).str();

    VS_LOG_INFO << msg << endl;

    return true;
}

void AmqpClient::setPollTimeout( int32_t _timeoutMillsec ){

    m_state.settings.serverPollTimeoutMillisec = _timeoutMillsec;
}

void AmqpClient::addObserver( INetworkObserver * _observer ){

    m_observers.push_back( _observer );
}

void AmqpClient::shutdown(){

    m_shutdownCalled.store( true );

    common_utils::threadShutdown( m_threadIncomingPackages );

    if( m_connTransmit ){
        ::amqp_destroy_connection( m_connTransmit );
    }
    if( m_connReceive ){
        ::amqp_destroy_connection( m_connReceive );
    }
}

bool AmqpClient::initLowLevel( amqp_connection_state_t & _connection, const SInitSettings & _params ){

    const char * host = _params.serverHost.c_str();
    const char * vhost = _params.amqpVirtualHost.c_str();
    const int port = _params.port;
    const char * login = _params.login.c_str();
    const char * pass = _params.pass.c_str();

    // connection
    _connection = ::amqp_new_connection();

    amqp_rpc_reply_t ret2 = ::amqp_get_rpc_reply( _connection );
    if( ret2.reply_type != AMQP_RESPONSE_NORMAL ){
        // TODO: ?
    }

    // socket
    amqp_socket_t * socket = ::amqp_tcp_socket_new( _connection );
    if( ! socket ){
        m_state.m_lastError = "AMQP socket creation failed";
        return false;
    }

    const int status = ::amqp_socket_open( socket, host, port );
    if( status != AMQP_STATUS_OK ){
        m_state.m_lastError = ( boost::format( "AMQP socket open failed: %1%" ) % strerror(errno) ).str();
        VS_LOG_ERROR << "NETWORK AMQP-C CLIENT SOCKET OPEN ERROR: " << amqpStatusStr( status ) << endl;
        return false;
    }

    // authorize
    amqp_rpc_reply_t ret = ::amqp_login( _connection, vhost, 0, AMQP_DEFAULT_FRAME_SIZE, 0, AMQP_SASL_METHOD_PLAIN, login, pass );
    if( ret.reply_type != AMQP_RESPONSE_NORMAL ){
        m_state.m_lastError = ( boost::format( "AMQP login failed: %1%" ) % amqpStrError(ret) ).str();
        return false;
    }

    // channel
    amqp_channel_open_ok_t * rt = ::amqp_channel_open( _connection, 1 );
    ret = amqp_get_rpc_reply( _connection );
    if( ret.reply_type != AMQP_RESPONSE_NORMAL ){
        m_state.m_lastError = ( boost::format( "AMQP channel (SEND) creation failed: %1%" ) % amqpStrError(ret) ).str();
        return false;
    }

    return true;
}

bool AmqpClient::createExchangePoint( const std::string & _exchangePointName, EExchangeType _exchangePointType ){

    const char * exchangePointType = g_exchangePointType[ (int)_exchangePointType ];

    // exchange
#ifdef Astra
    ::amqp_exchange_declare( m_connReceive, 1, amqp_cstring_bytes(_exchangePoint.c_str()), amqp_cstring_bytes(exchangeType), 0, 0, amqp_empty_table );
#else
    amqp_exchange_declare_ok_t * declareOk = ::amqp_exchange_declare( m_connReceive, 1, amqp_cstring_bytes(_exchangePointName.c_str()), amqp_cstring_bytes(exchangePointType), 0, 0, 0, 0, amqp_empty_table );
#endif

    amqp_rpc_reply_t ret = ::amqp_get_rpc_reply( m_connReceive );
    if( ret.reply_type != AMQP_RESPONSE_NORMAL ){
        m_state.m_lastError = ( boost::format( "AMQP exchange creation failed: %1%" ) % amqpStrError(ret) ).str();
        return false;
    }

    VS_LOG_INFO << PRINT_HEADER
                << " created EP [" << _exchangePointName << "]"
                << endl;

    return true;
}

bool AmqpClient::createMailbox( const string & _exchangePointName,
                                const string & _queueName,
                                const string _bindingKeyName,
                                bool _startConsume ){
    // queue
    amqp_boolean_t passive = 0;
    amqp_boolean_t durable = 0;
    amqp_boolean_t exclusive = 0;
    amqp_boolean_t auto_delete = 1;
    amqp_channel_t channelId = 1;

    amqp_queue_declare_ok_t * queueOk = ::amqp_queue_declare(   m_connReceive,
                                                                channelId,
                                                                amqp_cstring_bytes(_queueName.c_str()),
                                                                passive,
                                                                durable,
                                                                exclusive,
                                                                auto_delete,
                                                                amqp_empty_table );

    amqp_rpc_reply_t ret = ::amqp_get_rpc_reply( m_connReceive );
    if( ret.reply_type != AMQP_RESPONSE_NORMAL ){
        m_state.m_lastError = ( boost::format( "AMQP queue creation failed: %1%" ) % amqpStrError(ret) ).str();
        return false;
    }

    // bind
    ::amqp_queue_bind(  m_connReceive,
                        channelId,
                        amqp_cstring_bytes(_queueName.c_str()),
                        amqp_cstring_bytes(_exchangePointName.c_str()),
                        amqp_cstring_bytes(_bindingKeyName.c_str()),
                        amqp_empty_table );

    ret = ::amqp_get_rpc_reply( m_connReceive );
    if( ret.reply_type != AMQP_RESPONSE_NORMAL ){
        m_state.m_lastError = ( boost::format( "AMQP queue bind failed: %1%" ) % amqpStrError(ret) ).str();
        return false;
    }

    // subscribe
    if( _startConsume ){
        amqp_basic_consume_ok_t_ * consumeOk = ::amqp_basic_consume( m_connReceive,
                                                                   1,
                                                                   amqp_cstring_bytes(_queueName.c_str()),
                                                                   amqp_empty_bytes,
                                                                   0,
                                                                   1,
                                                                   0,
                                                                   amqp_empty_table );

        ret = ::amqp_get_rpc_reply( m_connReceive );
        if( ret.reply_type != AMQP_RESPONSE_NORMAL ){
            m_state.m_lastError = ( boost::format( "AMQP basic consume failed: %1%" ) % amqpStrError(ret) ).str();
            return false;
        }

        VS_LOG_INFO << PRINT_HEADER
                    << " starts consuming"
                    << " from Q [" << _queueName << "]"
                    << " connected to EP [" << _exchangePointName << "]"
                    << " by RK [" << _bindingKeyName << "]"
                    << endl;
    }
    else{
        VS_LOG_INFO << PRINT_HEADER
                    << " new Q [" << _queueName << "]"
                    << " connected to EP [" << _exchangePointName << "]"
                    << " by RK [" << _bindingKeyName << "]"
                    << endl;
    }

    return true;
}

void AmqpClient::threadReceiveLoop(){

    while( ! m_shutdownCalled ){
        poll();
    }
}

void AmqpClient::runNetworkCallbacks(){

    poll();
}

void AmqpClient::poll(){

    timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = m_state.settings.serverPollTimeoutMillisec * 1000;

    amqp_envelope_t envelope;
    amqp_rpc_reply_t ret = ::amqp_consume_message( m_connReceive, & envelope, & timeout, 0 );

    string corrId;
    string replyTo;

    switch( ret.reply_type ){
    case AMQP_RESPONSE_NORMAL: {
        corrId.assign( (char*)envelope.message.properties.correlation_id.bytes, envelope.message.properties.correlation_id.len );
        replyTo.assign( (char*)envelope.message.properties.reply_to.bytes, envelope.message.properties.reply_to.len );

//        VS_LOG_INFO << PRINT_HEADER
//                    << common_utils::getCurrentDateTimeStr()
//                    << " msg [" << string( (char*)envelope.message.body.bytes, envelope.message.body.len ) << "] consumed"
//                    << " reply to [" << replyTo << "]"
//                    << " corr id [" << corrId << "]"
//                    << endl;

        // (catched by client) response to async request
        if( m_readyResponsesToAsyncMessages.find(corrId) != m_readyResponsesToAsyncMessages.end() ){            
            if( m_refusedMessages.find(corrId) == m_refusedMessages.end() ){
                m_readyResponsesToAsyncMessages[ corrId ] = string( (char*)envelope.message.body.bytes, envelope.message.body.len );

                if( m_syncRequestPerformed && corrId == m_syncRequestCorrelationId ){
                    m_cvResponseToBlockedRequestArrived.notify_one();
                }
            }
            else{
                VS_LOG_WARN << PRINT_HEADER << " request corr id [" <<corrId << "] will be refused" << endl;
                m_refusedMessages.erase( corrId );
            }
        }
        // (catched by server) initiative from other side
        else{
            PAmqpRequest request = std::make_shared<AmqpRequest>();
            request->networkClient = this;
            request->m_connectionId = INetworkEntity::getConnId();
            request->m_incomingMessage.assign( (char*)envelope.message.body.bytes, envelope.message.body.len );
            request->m_correlationId.assign( (char*)envelope.message.properties.correlation_id.bytes, envelope.message.properties.correlation_id.len );
            request->replyTo = replyTo;

            for( INetworkObserver * observer : m_observers ){
                observer->callbackNetworkRequest( request );
            }
        }
        break;
    }
    case AMQP_RESPONSE_SERVER_EXCEPTION: {
        VS_LOG_WARN << "server exception reply id: " << ret.reply.id << endl;
    //        case AMQP_CONNECTION_CLOSE_METHOD: {
    //            amqp_connection_close_t *m = (amqp_connection_close_t *) ret.reply.decoded;
    //            out << "Server connection error " << m->reply_code << ", message: " << std::string((char *) m->reply_text.bytes, m->reply_text.len);
    //            break;
    //        }
        break;
    }
    case AMQP_RESPONSE_LIBRARY_EXCEPTION: {
//        VS_LOG_WARN << PRINT_HEADER << " lib exception reply err [" << amqp_error_string2(ret.library_error) << "]" << endl;
        break;
    }
    default: {
//      "lib err: " << ret.library_error
        // NOTE: if ret.library_error == AMQP_STATUS_TIMEOUT -> Operation timed out
        break;
    }
    }

    ::amqp_destroy_envelope( & envelope );
}

PEnvironmentRequest AmqpClient::getRequestInstance(){

    PAmqpRequest r = std::make_shared<AmqpRequest>();
    r->networkClient = this;

    return r;
}

bool AmqpClient::sendPackageAsync( const string & _msg,
                                   const string & _corrId,
                                   const std::string & _exchangeName,
                                   const std::string & _routingName,
                                   const std::string & _replyTo ){

    assert( ! _msg.empty() );
    assert( ! _corrId.empty() );
    assert( ! _exchangeName.empty() );
    assert( ! _routingName.empty() );

    std::lock_guard<std::mutex> lock( m_muSendAsync );

    // publish options
    constexpr amqp_channel_t channelId = 1;
    constexpr amqp_boolean_t mandatory = 1;
    constexpr amqp_boolean_t immediate = 0;

    // message options
    amqp_basic_properties_t_ props;
    props._flags = AMQP_BASIC_CORRELATION_ID_FLAG | AMQP_BASIC_REPLY_TO_FLAG | AMQP_BASIC_EXPIRATION_FLAG;
    props.correlation_id = amqp_cstring_bytes( _corrId.c_str() );
    props.reply_to = amqp_cstring_bytes( _replyTo.c_str() );
    props.expiration = amqp_cstring_bytes( m_msgExpirationMillisecStr.c_str() );

    // message itself
    amqp_bytes_t message_bytes;
    message_bytes.bytes = ( void * )_msg.data();
    message_bytes.len = _msg.size();

    //
    const amqp_status_enum_ publishStatus = (amqp_status_enum_)::amqp_basic_publish(  m_connTransmit,
                                                                    channelId,
                                                                    amqp_cstring_bytes( _exchangeName.c_str() ),
                                                                    amqp_cstring_bytes( _routingName.c_str() ),
                                                                    mandatory,
                                                                    immediate,
                                                                    & props,
                                                                    message_bytes );
    if( publishStatus != AMQP_STATUS_OK ){
        VS_LOG_ERROR << PRINT_HEADER << " message publish failed, reason [" << amqpStatusStr( publishStatus ) << "]" << endl;
        return false;
    }

    //
    m_readyResponsesToAsyncMessages.insert( {_corrId, string()} );

//    VS_LOG_INFO << PRINT_HEADER
//                << common_utils::getCurrentDateTimeStr()
//                << " msg [" << _msg << "] published"
//                << " to EP [" << _exchangeName << "]"
//                << " with RK [" << _routingName << "]"
//                << " corr id [" << _corrId << "]"
//                << endl;

    return true;
}

std::string AmqpClient::sendPackageBlocked( const string & _msg,
                                            const std::string & _corrId,
                                            const std::string & _exchangeName,
                                            const std::string & _routingName,
                                            const std::string & _replyTo ){

    std::lock_guard<std::mutex> lock( m_muSendBlocked );

    m_syncRequestPerformed = true;
    m_syncRequestCorrelationId = _corrId;

    // publish options
    constexpr amqp_channel_t channelId = 1;
    constexpr amqp_boolean_t mandatory = 1;
    constexpr amqp_boolean_t immediate = 0;

    // message options
    amqp_basic_properties_t_ props;
    props._flags = AMQP_BASIC_CORRELATION_ID_FLAG | AMQP_BASIC_REPLY_TO_FLAG | AMQP_BASIC_EXPIRATION_FLAG;
    props.correlation_id = amqp_cstring_bytes( _corrId.c_str() );
    props.reply_to = amqp_cstring_bytes( _replyTo.c_str() );
    props.expiration = amqp_cstring_bytes( m_msgExpirationMillisecStr.c_str() );

    // message itself
    amqp_bytes_t message_bytes;
    message_bytes.bytes = ( void * )_msg.data();
    message_bytes.len = _msg.size();

    // perform
    const amqp_status_enum_ publishStatus = (amqp_status_enum_)::amqp_basic_publish(  m_connTransmit,
                                                                    channelId,
                                                                    amqp_cstring_bytes( _exchangeName.c_str() ),
                                                                    amqp_cstring_bytes( _routingName.c_str() ),
                                                                    mandatory,
                                                                    immediate,
                                                                    & props,
                                                                    message_bytes );
    if( publishStatus != AMQP_STATUS_OK ){
        VS_LOG_ERROR << PRINT_HEADER << " message publish failed, reason [" << amqpStatusStr( publishStatus ) << "]" << endl;
        return string();
    }

    std::mutex muCvLock;
    std::unique_lock<std::mutex> cvLock( muCvLock );
    m_cvResponseToBlockedRequestArrived.wait_for(   cvLock,
                                                    std::chrono::milliseconds(m_state.settings.syncRequestTimeoutMillisec),
                                                    [ this, & _corrId ](){ return m_readyResponsesToAsyncMessages.find(_corrId) != m_readyResponsesToAsyncMessages.end(); } );

    // cathed ( may be )
    string response;
    auto iter = m_readyResponsesToAsyncMessages.find( _corrId );
    if( iter != m_readyResponsesToAsyncMessages.end() ){
        response = iter->second;
        m_readyResponsesToAsyncMessages.erase( _corrId );
    }

    m_syncRequestPerformed = false;
    m_syncRequestCorrelationId.clear();
    return response;
}

void AmqpClient::refuseFromResponse( const std::string & _corrId ){

    // TODO: do ?
}

bool AmqpClient::checkResponseReadyness( const std::string & _corrId ){

    auto iter = m_readyResponsesToAsyncMessages.find(_corrId);

    return ( iter != m_readyResponsesToAsyncMessages.end() && iter->second != string() );
}

std::string AmqpClient::getAsyncResponse( const std::string & _corrId ){

    assert( m_readyResponsesToAsyncMessages.find(_corrId) != m_readyResponsesToAsyncMessages.end() );

    const string out = m_readyResponsesToAsyncMessages[ _corrId ];
    m_readyResponsesToAsyncMessages.erase( _corrId );
    return out;
}















#ifdef OBJREPR_LIBRARY_EXIST

#include <objrepr/reprServer.h>

#include "objrepr_listener.h"
#include "system/logger.h"

using namespace std;
using namespace common_types;

// NOTE: watch for buffer overflow
static constexpr int OUTCOMING_BUFFER_SIZE = 4096;
static constexpr const char * PRINT_HEADER = "ObjreprServiceBus:";
static constexpr const char * MESSAGE_CONTENT_TYPE_SERVER = "json/server";
static constexpr const char * MESSAGE_CONTENT_TYPE_CLIENT = "json/client";
static constexpr const char * DELIMETER = "$$";

// ------------------------------------------------------------------
// request override
// ------------------------------------------------------------------
class ObjreprListenerRequest : public AEnvironmentRequest {
public:
    ObjreprListenerRequest()    
    {}

    virtual void setOutcomingMessage( const std::string & _msg ) override {

//        VS_LOG_INFO << "msg to client [" << _msg << "]" << endl;
        if( objreprListener->m_settings.serverMode ){

        }
        else{
//            if( objreprListener->m_settings.withPackageHeader ){
//                SNetworkPackage package;
//                package.header.m_clientInitiative = AEnvironmentRequest::m_clientInitiative;
//                package.msg = _msg;
//                AEnvironmentRequest::m_incomingMessage = objreprListener->sendBlockedRequest( package );
//            }
//            else{
//                AEnvironmentRequest::m_incomingMessage = objreprListener->sendBlockedRequest( _msg );
//            }
//            return true;
        }

        // TODO: this is a server mode
        if( objreprListener->m_settings.withPackageHeader ){
            SNetworkPackage package;
            package.header = m_header;
            package.msg = _msg;
            objreprListener->sendAsyncRequest( package );
        }
        else{
            const bool rt = objreprListener->m_listenedObject->sendServiceMessage( _msg, MESSAGE_CONTENT_TYPE_SERVER );
            if( ! rt ){
                VS_LOG_ERROR << PRINT_HEADER << " couldn't send service msg [" << _msg << "]" << endl;
                return;
            }
        }
    }

    virtual void * getUserData() override {
        return (void *)( & m_sensorId );
    }

    TSensorId m_sensorId;
    SNetworkPackage::SHeader m_header;
    ObjreprListener * objreprListener;
};
using PObjreprListenerRequest = std::shared_ptr<ObjreprListenerRequest>;

// ------------------------------------------------------------------
// transporter
// ------------------------------------------------------------------
ObjreprListener::ObjreprListener( INetworkEntity::TConnectionId _id )
    : INetworkProvider(_id)
    , INetworkClient(_id)
{
    m_outcomingBuffer = new char[ OUTCOMING_BUFFER_SIZE ];
    m_outcomingBufferSize = OUTCOMING_BUFFER_SIZE;
}

ObjreprListener::~ObjreprListener()
{
    shutdown();
}

bool ObjreprListener::init( SInitSettings _settings ){

    m_settings = _settings;

    //
    objrepr::SpatialObjectManager * objManager = objrepr::RepresentationServer::instance()->objectManager();
    objrepr::SpatialObjectPtr sensor = objManager->getObject( _settings.listenedObjectId );
    if( ! sensor ){
        VS_LOG_ERROR << PRINT_HEADER << " object for listen with id [" << _settings.listenedObjectId << "] not found" << endl;
        return false;
    }

    m_listenedObject = sensor;

    //
    const bool rt = m_listenedObject->subscribeOnServiceMessages();
    if( ! rt ){
        VS_LOG_ERROR << PRINT_HEADER << " cannot subscribe on object [" << m_listenedObject->name() << "]" << endl;
        return false;
    }

    //
    if( _settings.serverMode ){
        m_listenedObject->serviceMessageReceived.connect( boost::bind( & ObjreprListener::callbackFromObjreprServerMode, this, _1 ) );
    }
    else{
        m_listenedObject->serviceMessageReceived.connect( boost::bind( & ObjreprListener::callbackFromObjreprClientMode, this, _1 ) );
    }

    VS_LOG_INFO << PRINT_HEADER << " subscribed on object [" << m_listenedObject->name() << "]" << endl;

    return true;
}

void ObjreprListener::callbackFromObjreprServerMode( uint32_t _messageId ){

    std::string message;
    std::string contentType;
    constexpr uint32_t timeout = 0;
    const bool rt = m_listenedObject->recvServiceMessage( & message, & contentType, _messageId, timeout );
    if( ! rt ){
        VS_LOG_WARN << PRINT_HEADER << " receive message failed" << endl;
    }

    if( contentType != MESSAGE_CONTENT_TYPE_CLIENT ){
//        VS_LOG_WARN << PRINT_HEADER
//                    << " content type mismatch from message id [" << _messageId << "]"
//                    << " incoming type [" << contentType << "] with [" << MESSAGE_CONTENT_TYPE_CLIENT << "]"
//                    << " msg [" << message << "]"
//                    << endl;
        return;
    }

    if( message.empty() ){
        return;
    }

    // request from client with header
    if( m_settings.withPackageHeader ){
        SNetworkPackage * package = (SNetworkPackage *)message.data();

        PObjreprListenerRequest request = std::make_shared<ObjreprListenerRequest>();
        request->m_incomingMessage.assign( ((char *)message.data()) + sizeof(SNetworkPackage::SHeader), message.size() - 1 );
        request->m_sensorId = m_listenedObject->id();
        request->m_connectionId = INetworkEntity::getConnId();
        request->m_header = package->header;
        request->objreprListener = this;

        for( INetworkObserver * observer : m_observers ){
            observer->callbackNetworkRequest( request );
        }        
    }
    // request from client w/o header
    else{
        PObjreprListenerRequest request = std::make_shared<ObjreprListenerRequest>();
        request->m_incomingMessage = message;
        request->m_sensorId = m_listenedObject->id();
        request->m_connectionId = INetworkEntity::getConnId();
        request->objreprListener = this;

        for( INetworkObserver * observer : m_observers ){
            observer->callbackNetworkRequest( request );
        }
    }
}

void ObjreprListener::callbackFromObjreprClientMode( uint32_t _messageId ){

    std::string message;
    std::string contentType;
    constexpr uint32_t timeout = 0;
    const bool rt = m_listenedObject->recvServiceMessage( & message, & contentType, _messageId, timeout );
    if( ! rt ){
        VS_LOG_WARN << PRINT_HEADER << " receive message failed" << endl;
    }

    if( contentType != MESSAGE_CONTENT_TYPE_SERVER ){
        VS_LOG_WARN << PRINT_HEADER
                    << " content type mismatch from message id [" << _messageId << "]"
                    << " incoming type [" << contentType << "] with [" << MESSAGE_CONTENT_TYPE_SERVER << "]"
                    << " msg [" << message << "]"
                    << endl;
        return;
    }

    if( message.empty() ){
        return;
    }

    if( m_settings.withPackageHeader ){
        SNetworkPackage * package = (SNetworkPackage *)message.data();

        // server initiative
        if( ! package->header.m_clientInitiative ){
            PObjreprListenerRequest request = std::make_shared<ObjreprListenerRequest>();
            request->m_sensorId = m_listenedObject->id();
            request->m_connectionId = INetworkEntity::getConnId();
            request->m_incomingMessage.assign( ((char *)message.data()) + sizeof(SNetworkPackage::SHeader), message.size() - 1 );
            request->m_header = package->header;
            request->objreprListener = this;

            for( INetworkObserver * observer : m_observers ){
                observer->callbackNetworkRequest( request );
            }

            VS_LOG_INFO << PRINT_HEADER
                        << " message from server (initiator) [" << request->m_incomingMessage << "]"
                        << endl;
        }
        // response from server
        else{
            m_incomingMessageData.assign( ((char *)message.data()) + sizeof(SNetworkPackage::SHeader), message.size() - 1 );
            m_responseCatched.store( true );
            m_cvResponseCame.notify_one();

            if( m_incomingMessageData.find("pong") == string::npos ){
                VS_LOG_INFO << PRINT_HEADER
                            << " message from server (response) [" << m_incomingMessageData << "]"
                            << endl;
            }
        }
    }
    else{
        // TODO: server's initiative ( w/o header )


        // NOTE: this is a response
        m_incomingMessageData = message;
        m_responseCatched.store( true );
        m_cvResponseCame.notify_one();

        if( m_incomingMessageData.find("pong") == string::npos ){
            VS_LOG_INFO << PRINT_HEADER
                        << " message from server (response) [" << m_incomingMessageData << "]"
                        << endl;
        }
    }
}

void ObjreprListener::addObserver( INetworkObserver * _observer ){
    m_observers.push_back( _observer );
}

void ObjreprListener::removeObserver( INetworkObserver * _observer ){

    for( auto iter = m_observers.begin(); iter != m_observers.end(); ){
        INetworkObserver * observer = ( * iter );

        if( observer == _observer ){
            iter = m_observers.erase( iter );
            return;
        }
        else{
            ++iter;
        }
    }
}

void ObjreprListener::runNetworkCallbacks(){

    // NOTE: dummy
}

void ObjreprListener::setPollTimeout( int32_t _timeoutMillsec ){

}

PEnvironmentRequest ObjreprListener::getRequestInstance(){

    PObjreprListenerRequest request = std::make_shared<ObjreprListenerRequest>();
    request->objreprListener = this;
    request->m_sensorId = m_listenedObject->id();
    return request;
}

void ObjreprListener::shutdown(){

    const bool rt = m_listenedObject->unsubscribeFromServiceMessages();

    m_listenedObject->serviceMessageReceived.disconnect_all_slots();

    delete[] m_outcomingBuffer;
}

std::string ObjreprListener::sendBlockedRequest( const SNetworkPackage & _package ){

    std::lock_guard<std::mutex> lock( m_mutexSendProtection );
    m_incomingMessageData.clear();

    // adjust buffer size
    if( _package.msg.size() > (m_outcomingBufferSize + 1) ){
        delete[] m_outcomingBuffer;
        m_outcomingBuffer = new char[ m_outcomingBufferSize * 2 ];
        m_outcomingBufferSize *= 2;
    }

    // send request
    ::memcpy( m_outcomingBuffer, & _package.header, sizeof(SNetworkPackage::SHeader) );
    char * messageSection = m_outcomingBuffer + sizeof(SNetworkPackage::SHeader);
    ::memcpy( messageSection, _package.msg.data(), _package.msg.size() );

    m_responseCatched.store( false );

    const string toSend( m_outcomingBuffer, sizeof(SNetworkPackage::SHeader) + _package.msg.size() );
    const bool rt = m_listenedObject->sendServiceMessage( toSend, MESSAGE_CONTENT_TYPE_SERVER );
    if( ! rt ){
        VS_LOG_ERROR << PRINT_HEADER << " couldn't send service msg [" << toSend << "]" << endl;
        return m_incomingMessageData;
    }

    // wait response
    std::mutex lockMutex;
    std::unique_lock<std::mutex> cvLock( lockMutex );
    m_cvResponseCame.wait_for( cvLock,
                               std::chrono::milliseconds(m_settings.responseWaitTimeoutMillisec),
                               [ this ](){ return m_responseCatched.load(); } );

    m_responseCatched.store( false );

    if( m_incomingMessageData.empty() ){
        VS_LOG_WARN << PRINT_HEADER << " response wait timeouted, but message is empty" << endl;
    }

    // return reponse
    return m_incomingMessageData;
}

void ObjreprListener::sendAsyncRequest( const SNetworkPackage & _package ){

    std::lock_guard<std::mutex> lock( m_mutexSendProtection );

    // dynamic growing
    if( _package.msg.size() > (m_outcomingBufferSize + 1) ){
        delete[] m_outcomingBuffer;
        m_outcomingBuffer = new char[ m_outcomingBufferSize * 2 ];
        m_outcomingBufferSize *= 2;
    }

    //
    ::memcpy( m_outcomingBuffer, & _package.header, sizeof(SNetworkPackage::SHeader) );
    char * messageSection = m_outcomingBuffer + sizeof(SNetworkPackage::SHeader);
    ::memcpy( messageSection, _package.msg.data(), _package.msg.size() );

#if ENABLE_DEBUG_PRINTS
    if( _package.msg.find("\"message\":\"pong\"") == string::npos ){
        VS_LOG_DBG << PRINT_HEADER << " send msg [" << _package.msg << "]" << endl;
    }
#endif

    //
    const string toSend( m_outcomingBuffer, sizeof(SNetworkPackage::SHeader) + _package.msg.size() );
    const bool rt = m_listenedObject->sendServiceMessage( toSend, MESSAGE_CONTENT_TYPE_SERVER );
    if( ! rt ){
        VS_LOG_ERROR << PRINT_HEADER << " couldn't send service msg [" << toSend << "]" << endl;
        return;
    }
}

#endif



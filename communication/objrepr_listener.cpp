
#ifdef OBJREPR_LIBRARY_EXIST

#include <objrepr/spatialObject.h>

#include "system/logger.h"
#include "common/common_types.h"
#include "objrepr_listener.h"

using namespace std;

static constexpr const char * MESSAGE_CONTENT_TYPE = "text/json";
static constexpr const char * DELIMETER = "$$";

static inline string serializeToString( const SNetworkPackage & _pack ){

    string out;
    out.reserve( _pack.msg.size() );

    out.append( std::to_string( _pack.header.m_asyncRequestId ) );
    out.append( DELIMETER );
    out.append( std::to_string( _pack.header.m_fromClient ) );
    out.append( DELIMETER );
    out.append( _pack.msg );

    return out;
}

static inline SNetworkPackage deserializeFromString( const string & _str ){

    SNetworkPackage out;

    const string::size_type posFirstDollar = _str.find_first_of( DELIMETER );
    const string::size_type posSecondDollar = _str.find_last_of( DELIMETER );

    out.header.m_asyncRequestId = stoll( _str.substr( 0, posFirstDollar - 1 ) );
    out.header.m_fromClient = stoi( _str.substr( posFirstDollar + 2, posSecondDollar - 1 ) );
    out.msg = _str.substr( posSecondDollar + 2, _str.size() - posSecondDollar );

    return out;
}

// ------------------------------------------------------------------
// request override
// ------------------------------------------------------------------
class ObjreprListenerRequest : public AEnvironmentRequest {

public:
    ObjreprListenerRequest()
    {}

    virtual void setOutcomingMessage( const std::string & _msg ) override {

        // TODO: packaged transfer
//        SNetworkPackage package;
//        package.header = m_header;
//        package.msg = _msg;
//        const string toSend = serializeToString( package );

        const string toSend = _msg;
#ifdef OBJREPR_LIBRARY_EXIST
        bool rt = listenedObject->sendServiceMessage( toSend, MESSAGE_CONTENT_TYPE );
#endif

        // TODO: check rt
    }

    virtual void * getUserData() override {
        return (void *)( & m_sensorId );
    }

    common_types::TSensorId m_sensorId;
    SNetworkPackage::SHeader m_header;
#ifdef OBJREPR_LIBRARY_EXIST
    objrepr::SpatialObjectPtr listenedObject;
#endif

};
using PObjreprListenerRequest = std::shared_ptr<ObjreprListenerRequest>;

// ------------------------------------------------------------------
// transporter
// ------------------------------------------------------------------
ObjreprListener::ObjreprListener( INetworkEntity::TConnectionId _id )
    : INetworkProvider(_id)
    , INetworkClient(_id)
{

}

ObjreprListener::~ObjreprListener()
{
    shutdown();
}

bool ObjreprListener::init( SInitSettings _settings ){

    assert( _settings.listenedObject );

    m_settings = _settings;

    const bool rt = m_settings.listenedObject->subscribeOnServiceMessages();
    if( ! rt ){
        VS_LOG_ERROR << "ObjreprListener cannot subscribe on object [" << m_settings.listenedObject->name() << "]" << endl;
        return false;
    }

    m_settings.listenedObject->serviceMessageReceived.connect( boost::bind( & ObjreprListener::callbackFromObjrepr, this, _1 ) );

    VS_LOG_INFO << "ObjreprListener subscribed on object [" << m_settings.listenedObject->name() << "]" << endl;

    return true;
}

void ObjreprListener::callbackFromObjrepr( uint32_t _messageId ){

    std::string message;
    std::string contentType;
    constexpr uint32_t timeout = 0;
    m_settings.listenedObject->recvServiceMessage( & message, & contentType, _messageId, timeout );

    if( contentType != MESSAGE_CONTENT_TYPE ){
        VS_LOG_WARN << "content type mismatch from message id [" << _messageId << "]"
                 << " with [" << message << "]"
                 << endl;
        return;
    }

    // TODO: packaged transfer
//    const SNetworkPackage pack = deserializeFromString( message );
//    request->m_incomingMessage = pack.msg;

    PObjreprListenerRequest request = std::make_shared<ObjreprListenerRequest>();
    request->m_incomingMessage = message;
    request->m_sensorId = m_settings.listenedObject->id();
    request->m_connectionId = INetworkEntity::getConnId();

    for( INetworkObserver * observer : m_observers ){
        observer->callbackNetworkRequest( request );
    }

}

void ObjreprListener::addObserver( INetworkObserver * _observer ){
    m_observers.push_back( _observer );
}

void ObjreprListener::runNetworkCallbacks(){

    // NOTE: dummy
}

void ObjreprListener::setPollTimeout( int32_t _timeoutMillsec ){

}

PEnvironmentRequest ObjreprListener::getRequestInstance(){

    PObjreprListenerRequest request = std::make_shared<ObjreprListenerRequest>();
    request->listenedObject = m_settings.listenedObject;
    request->m_header.m_fromClient = true;
    return request;
}

void ObjreprListener::shutdown(){

    const bool rt = m_settings.listenedObject->unsubscribeFromServiceMessages();

    m_settings.listenedObject->serviceMessageReceived.disconnect_all_slots();
}

#endif





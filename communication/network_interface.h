#ifndef NETWORK_INTERFACE_H
#define NETWORK_INTERFACE_H

#include <cassert>
#include <atomic>
#include <string>
#include <memory>
#include <vector>


// ------------------------------------
// TYPEDEFS
// ------------------------------------
using TAsyncRequestId = uint64_t;
using TCorrelationId = std::string;

struct SNetworkPackage {
    struct SHeader {
        SHeader()
            : m_clientInitiative(false)
            , m_asyncRequestId(0)
        {}
        // NOTE: not put data types with heap allocation, only POD types !
        bool m_clientInitiative;
        TAsyncRequestId m_asyncRequestId;
    };

    SNetworkPackage()
    {}
    SHeader header;
    std::string msg;
};

struct SAmqpRouteParameters {
    SAmqpRouteParameters()
    {}
    std::string predatorExchangePointName;
    std::string predatorQueueName;
    std::string predatorRoutingKeyName;

    std::string targetExchangePointName;
    std::string targetQueueName;
    std::string targetRoutingKeyName;
};


// ------------------------------------
// COMMON NETWORK FEATURES
// ------------------------------------
class INetworkEntity {
public:
    using TConnectionId = int64_t;
    static const TConnectionId INVALID_CONN_ID;

    INetworkEntity()
    {}
    INetworkEntity( TConnectionId /*_connId*/ _id )
        : m_connId(_id)
    {}
    virtual ~INetworkEntity(){}

    virtual bool isConnectionEstablished() { return false; }

    TConnectionId getConnId() const { return m_connId; }


protected:
    TConnectionId m_connId;
};
using PNetworkEntity = std::shared_ptr<INetworkEntity>;


// ------------------------------------
// REQUEST
// ------------------------------------
class AEnvironmentRequest {
public:
    // TODO: do ?
    enum EFlags : uint8_t {
        F_NEED_RESPONSE = 0x01,
        F_ASYNC_REQUEST = 0x02,
        F_NOTIFY_ABOUT_ASYNC_VIA_CALLBACK = 0x04,
        F_RESPONSE_CATCHED = 0x08,

        F_RESERVE_1 = 0x10,
        F_RESERVE_2 = 0x20,
        F_RESERVE_3 = 0x40,
        F_RESERVE_4 = 0x80
    };

    uint8_t getFlags(){ return m_flags; }
    void setFlags( uint8_t _flags ){ m_flags = _flags; }

    uint8_t m_flags;

    AEnvironmentRequest()
        : m_connectionId(0)
        , m_asyncRequest(false)
        , m_notifyAboutAsyncViaCallback(false)
        , m_flags(0)
        , m_timeouted(false)
        , m_requestTimeMillisec(0)
    {}

    // sync payload
    const std::string & getIncomingMessage(){ return m_incomingMessage; }

    virtual void setOutcomingMessage( const std::string & _msg ) = 0;
    virtual void setOutcomingMessage( const char * _bytes, int _bytesLen ) { assert( false && "not implemented in derived class" ); }

    // async payload
    virtual std::string sendMessageAsync( const std::string & _msg, const std::string & _correlationId = "" ){ assert( false && "not implemented in derived class" ); }
    virtual bool checkResponseReadyness(){ assert( false && "not implemented in derived class" ); }
    virtual std::string getAsyncResponse(){ assert( false && "not implemented in derived class" ); }
    bool isTimeouted(){ return m_timeouted; }
    bool isPerforming(){ return ! m_correlationId.empty(); }

    // service
    INetworkEntity::TConnectionId getConnId(){ return m_connectionId; }
    virtual void setUserData( void * /*_data*/ ){ return; }
    virtual void * getUserData(){ return nullptr; }


    // TODO: to private/protected

    // TODO: deprecated
    bool m_asyncRequest;   
    bool m_notifyAboutAsyncViaCallback;

    INetworkEntity::TConnectionId m_connectionId;
    std::string m_incomingMessage;

    TCorrelationId m_correlationId;
    bool m_timeouted;
    int64_t m_requestTimeMillisec;


protected:


private:


};
using PEnvironmentRequest = std::shared_ptr<AEnvironmentRequest>;


// ------------------------------------
// SERVER
// ------------------------------------
class INetworkObserver {
public:
    virtual ~INetworkObserver(){}

    // TODO: ?
    virtual void callbackConnectionEstablished( bool /*_established*/, INetworkEntity::TConnectionId /*_connId*/ ) {}
    virtual void callbackNetworkRequest( PEnvironmentRequest _request ) = 0;
};

class INetworkProvider : virtual public INetworkEntity {
public:
    INetworkProvider( INetworkEntity::TConnectionId _id )
        : INetworkEntity(_id)
    {}
    virtual ~INetworkProvider(){}

    virtual void shutdown() = 0;
    virtual void runNetworkCallbacks() = 0;
    virtual void addObserver( INetworkObserver * _observer ) = 0;
    virtual void removeObserver( INetworkObserver * /*_observer*/ ) {}
    virtual void setPollTimeout( int32_t _timeoutMillsec ) = 0;

    virtual PEnvironmentRequest initiateRequestToConnection( INetworkEntity::TConnectionId ){ assert( false && "method not implemented by derive class" ); }
    virtual std::vector<INetworkEntity::TConnectionId> getChildConnections(){
        return std::vector<INetworkEntity::TConnectionId>();
    }

    virtual std::vector<INetworkEntity> getConnections() { assert( false && "method not implemented by derive class" ); }
};
using PNetworkProvider = std::shared_ptr<INetworkProvider>;


// ------------------------------------
// CLIENT
// ------------------------------------
class INetworkClient : virtual public INetworkEntity {
public:
    INetworkClient( INetworkEntity::TConnectionId _id )
            : INetworkEntity(_id)
        {}
    virtual ~INetworkClient(){}
    virtual PEnvironmentRequest getRequestInstance() = 0;
};
using PNetworkClient = std::shared_ptr<INetworkClient>;


// ------------------------------------
// network services
// ------------------------------------
class ICommunicationService {
public:
    virtual ~ICommunicationService(){}

    virtual PNetworkEntity getConnection( INetworkEntity::TConnectionId _connId ) = 0;
    virtual PNetworkClient getFileDownloader() = 0;
};

#endif // NETWORK_INTERFACE_H



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
            : m_fromClient(false)
            , m_asyncRequestId(0)
        {}
        // NOTE: not put data types with heap allocation, only POD types !
        bool m_fromClient;
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
    enum EPriority {
        PRIMARY,
        SECONDARY,
        UNDEFINED
    };

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

    AEnvironmentRequest()
        : m_connectionId(0)
        , m_asyncRequestId(0)
        , m_asyncRequest(false)
        , m_notifyAboutAsyncViaCallback(false)
        , m_responseCatched(false)
        , m_flags(0)
    {}

    const std::string & getIncomingMessage(){ return m_incomingMessage; }
    // TODO: may be remove 'bool _success' ?
    virtual void setOutcomingMessage( const std::string & _msg ) = 0;
    virtual void setOutcomingMessage( const char * _bytes, int _bytesLen ) { assert( false && "not implemented in derived class" ); }

    virtual void setUserData( void * /*_data*/ ){ return; }
    virtual void * getUserData(){ return nullptr; }

    uint8_t getFlags(){ return m_flags; }
    void setFlags( uint8_t _flags ){ m_flags = _flags; }

    void setPriority( EPriority _prio ){ m_priority = _prio; }
    void setAsyncMode( bool _async ){ m_asyncRequest = _async; }

    virtual std::string sendMessageAsync( const std::string & _msg, const std::string & _correlationId = "" ){ assert( false && "not implemented in derived class" ); }
    virtual bool checkResponseReadyness( const std::string & _correlationId ){ assert( false && "not implemented in derived class" ); }
    virtual std::string getAsyncResponse( const std::string & _correlationId ){ assert( false && "not implemented in derived class" ); }

    INetworkEntity::TConnectionId getConnId(){ return m_connectionId; }

    // ------------------ TODO: to private/protected ------------------

    // header to network facility
    SNetworkPackage m_netPack;
    // payload
    std::string m_incomingMessage;
    // to endpoint
    TAsyncRequestId m_asyncRequestId;
    // inside in the system
    bool m_asyncRequest;
    bool m_notifyAboutAsyncViaCallback;
    std::atomic<bool> m_responseCatched;
    uint8_t m_flags;
    EPriority m_priority;

    // TODO: refactored section
    INetworkEntity::TConnectionId m_connectionId;
    TCorrelationId m_correlationId;


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

#endif // NETWORK_INTERFACE_H



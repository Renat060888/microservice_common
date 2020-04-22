#ifndef OBJREPR_LISTENER_H
#define OBJREPR_LISTENER_H

#include <mutex>
#include <condition_variable>

#include <objrepr/spatialObject.h>

#include "common/ms_common_types.h"
#include "communication/network_interface.h"

class ObjreprListener : public INetworkProvider, public INetworkClient
{
    friend class ObjreprListenerRequest;
public:
    struct SInitSettings {
        SInitSettings()
            : listenedObjectId(0)
            , withPackageHeader(false)
            , responseWaitTimeoutMillisec(5000)
            , serverMode(true)
        {}
        common_types::TObjectId listenedObjectId;
        bool withPackageHeader;
        int responseWaitTimeoutMillisec;
        bool serverMode;
    };

    ObjreprListener( INetworkEntity::TConnectionId _id );
    ~ObjreprListener();

    bool init( SInitSettings _settings );

    // server interface
    virtual void addObserver( INetworkObserver * _observer ) override;
    virtual void removeObserver( INetworkObserver * _observer ) override;
    virtual void runNetworkCallbacks() override;
    virtual void setPollTimeout( int32_t _timeoutMillsec ) override;

    // client interface
    virtual PEnvironmentRequest getRequestInstance() override;

private:
    void callbackFromObjreprServerMode( uint32_t _messageId );
    void callbackFromObjreprClientMode( uint32_t _messageId );

    void sendAsyncRequest( const SNetworkPackage & _package );
    std::string sendBlockedRequest( const SNetworkPackage & _package );

    // TODO:
    void sendAsyncRequest( const std::string & _msg );
    std::string sendBlockedRequest( const std::string & _msg );

    virtual void shutdown() override;

    // data
    std::vector<INetworkObserver *> m_observers;
    SInitSettings m_settings;
    char * m_outcomingBuffer;
    int32_t m_outcomingBufferSize;
    std::atomic<bool> m_responseCatched;
    std::string m_incomingMessageData;

    // service
    std::mutex m_mutexSendProtection;
    objrepr::SpatialObjectPtr m_listenedObject;
    std::condition_variable m_cvResponseCame;
};
using PObjreprListener = std::shared_ptr<ObjreprListener>;

#endif // OBJREPR_LISTENER_H

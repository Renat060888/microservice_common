#ifndef OBJREPR_LISTENER_H
#define OBJREPR_LISTENER_H

//#define OBJREPR_LIBRARY_EXIST

#include "communication/network_interface.h"

#ifdef OBJREPR_LIBRARY_EXIST
class ObjreprListener : public INetworkProvider, public INetworkClient
{
public:
    struct SInitSettings {
        objrepr::SpatialObjectPtr listenedObject;
    };

    ObjreprListener( INetworkEntity::TConnectionId _id );
    ~ObjreprListener();

    bool init( SInitSettings _settings );

    // server interface
    void addObserver( INetworkObserver * _observer ) override;
    virtual void runNetworkCallbacks() override;
    virtual void setPollTimeout( int32_t _timeoutMillsec ) override;

    // client interface
    virtual PEnvironmentRequest getRequestInstance() override;

private:
    void callbackFromObjrepr( uint32_t _messageId );

    virtual void shutdown() override;

    // data
    std::vector<INetworkObserver *> m_observers;
    SInitSettings m_settings;



};
using PObjreprListener = std::shared_ptr<ObjreprListener>;
#endif

#endif // OBJREPR_LISTENER_H

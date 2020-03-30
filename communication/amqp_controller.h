#ifndef I_AMQP_CONTROLLER_H
#define I_AMQP_CONTROLLER_H

#include "network_interface.h"

class AmqpController : public INetworkClient, public INetworkProvider
{
public:    
    struct SInitSettings {
        PNetworkClient client;
        SAmqpRouteParameters route;
    };

    struct SState {
        SInitSettings settings;
        std::string lastError;
    };

    AmqpController( INetworkEntity::TConnectionId _id );
    ~AmqpController();

    bool init( const SInitSettings & _settings );
    const SState & getState(){ return m_state; }

    PNetworkClient getOwnedClient(){ return m_state.settings.client; }

    // client itf
    virtual PEnvironmentRequest getRequestInstance() override;

    // server itf
    virtual void shutdown() override;
    virtual void runNetworkCallbacks() override;
    virtual void setPollTimeout( int32_t _timeoutMillsec ) override;
    virtual void addObserver( INetworkObserver * _observer ) override;
    virtual void removeObserver( INetworkObserver * _observer ) override;


private:
    bool configureRoute( const SInitSettings & _settings );

    // data
    SState m_state;


    // service



};
using PAmqpController = std::shared_ptr<AmqpController>;

#endif // I_AMQP_CONTROLLER_H

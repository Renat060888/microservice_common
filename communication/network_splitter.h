#ifndef NETWORK_SPLITTER_H
#define NETWORK_SPLITTER_H

#include "network_interface.h"

class NetworkSplitter : public INetworkProvider, public INetworkClient
{
public:
    struct SInitSettings {
        PNetworkClient m_realCommunicator;
    };

    NetworkSplitter( INetworkEntity::TConnectionId _id );

    bool init( SInitSettings _settings );
    void addObserver( INetworkObserver * _observer ) override;
    void removeObserver( INetworkObserver * _observer ) override;

    // server interface
    virtual void runNetworkCallbacks() override;
    virtual void setPollTimeout( int32_t _timeoutMillsec ) override;

    // client interface
    virtual PEnvironmentRequest getRequestInstance() override;

private:
    virtual void shutdown() override;


    // data
    SInitSettings m_settings;



    // service

};
using PNetworkSplitter = std::shared_ptr<NetworkSplitter>;

#endif // NETWORK_SPLITTER_H

#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <thread>
#include <condition_variable>
#include <atomic>
#include <string>
#include <vector>

#include "network_interface.h"

class WebServer : public INetworkProvider
{
public:
    struct SInitSettings {
        SInitSettings()
            : listenPort(0)
            , async(false)
            , pollTimeoutMillisec(10)
            , commandConvertor(nullptr)
        {}
        bool async;
        int listenPort;
        int pollTimeoutMillisec;
        class UnifiedCommandConvertor * commandConvertor;
    };

    WebServer( INetworkEntity::TConnectionId _id );
    ~WebServer();

    bool init( SInitSettings _settings );
    void addObserver( INetworkObserver * _observer ) override;
    virtual void runNetworkCallbacks() override;
    virtual void setPollTimeout( int32_t _timeoutMillsec ) override;

    struct SWebServerPrivateImplementation * m_impl;

private:
    virtual void shutdown() override;

    void threadWebServerListen();    

};
using PWebserver = std::shared_ptr<WebServer>;

#endif // WEBSERVER_H


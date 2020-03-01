#ifndef COMMUNICATION_GATEWAY_H
#define COMMUNICATION_GATEWAY_H

#include <queue>
#include <thread>
#include <mutex>
#include <map>
#include <unordered_map>

#include "i_command.h"
#include "i_command_factory.h"
#include "i_amqp_controller.h"
#include "network_interface.h"
#include "common/ms_common_types.h"

class CommunicationGatewayFacade :  public INetworkObserver, public common_types::ICommunicationService
{
public:    
    // available transport mechanisms
    struct SConnectParamsAmqp {
        SConnectParamsAmqp()
            : enable(false)
            , port(0)
        {}
        bool enable;
        std::string host;
        std::string virtHost;
        int port;
        std::string login;
        std::string pass;
        SAmqpRouteParameters route;
    };

    struct SConnectParamsShell {
        SConnectParamsShell()
            : enable(false)
            , client(false)
        {}
        bool enable;
        bool client;
        std::string socketName;
    };

    struct SConnectParamsHTTPClient {
        SConnectParamsHTTPClient()
            : enable(false)
            , port(0)
        {}
        bool enable;
        std::string host;
        int port;
    };

    struct SConnectParamsHTTPServer {
        SConnectParamsHTTPServer()
            : enable(false)
            , port(0)
        {}
        bool enable;
        int port;
    };

    struct SConnectParamsWebsocketServer {
        SConnectParamsWebsocketServer()
            : enable(false)
            , port(0)
        {}
        bool enable;
        int port;
    };

    struct SConnectParamsSharedMem {
        SConnectParamsSharedMem()
            : enable(false)
        {}
        bool enable;
        std::string memoryAreaName;
    };

    // specialization
    struct SSpecParameters {
        SSpecParameters()
            : factory(nullptr)
        {}
        ICommandFactory * factory;
    };

    // main parameters
    struct SInitSettings {
        SInitSettings()
            : asyncNetwork(false)
            , pollTimeoutMillisec(0)
        {}
        // configured by client code
        mutable std::vector<PEnvironmentRequest> requestsFromWAL;
        mutable std::vector<PEnvironmentRequest> requestsFromConfig;
        bool asyncNetwork;
        int32_t pollTimeoutMillisec;

        // configured by derived class
        SConnectParamsAmqp paramsForInitialAmqp;
        SConnectParamsShell paramsForInitialShell;
        SConnectParamsHTTPClient paramsForInitialHTTPClient;
        SConnectParamsHTTPServer paramsForInitialHTTPServer;
        SConnectParamsWebsocketServer paramsForInitialWebsocketServer;
        SConnectParamsSharedMem paramsForInitialSharedMem;

        SSpecParameters specParams;
    };

    //
    CommunicationGatewayFacade();
    ~CommunicationGatewayFacade();

    //
    bool init( const SInitSettings & _settings );
    const SInitSettings & getInitSettings(){ return m_settings; }
    void shutdown();

    // commands processing
    void runNetworkCallbacks();
    bool isCommandAvailable();
    PCommand nextIncomingCommand();

    // NOTE: custom services will be in derived classes


protected:
    virtual bool initDerive( const SInitSettings & _settings ){ return true; }

    PNetworkClient getNewAmqpConnection( const SConnectParamsAmqp & _params );
    PNetworkClient getNewShellConnection( const SConnectParamsShell & _params );
    PNetworkClient getNewHTTPClientConnection( const SConnectParamsHTTPClient & _params );
    PNetworkProvider getNewHTTPServerConnection( const SConnectParamsHTTPServer & _params );
    PNetworkProvider getNewWebsocketServerConnection( const SConnectParamsWebsocketServer & _params );
    PNetworkClient getNewSharedMemConnection( const SConnectParamsSharedMem & _params );

    PNetworkClient getInitialAmqpConnection();
    PNetworkClient getInitialShellConnection();
    PNetworkClient getInitialHTTPClientConnection();
    PNetworkProvider getInitialHTTPServerConnection();
    PNetworkProvider getInitialWebsocketServerConnection();
    PNetworkClient getInitialSharedMemConnection();

    INetworkEntity::TConnectionId getConnectionId();


private:
    virtual void callbackNetworkRequest( PEnvironmentRequest _request ) override;
    void threadNetworkCallbacks();

    virtual PNetworkEntity getConnection( INetworkEntity::TConnectionId _connId ) override;
    virtual PNetworkClient getFileDownloader() override;

    bool initialConnections( const SInitSettings & _settings );

    // TODO: to private impl

    // data
    INetworkEntity::TConnectionId m_connectionIdGenerator;
    bool m_shutdownCalled;
    SInitSettings m_settings;

    std::vector<PNetworkClient> m_clientNetworks;
    std::vector<PNetworkProvider> m_serverNetworks;

    std::vector<PNetworkProvider> m_externalNetworks;
    std::unordered_map<INetworkEntity::TConnectionId, PNetworkProvider> m_externalNetworksById;
    std::vector<PNetworkProvider> m_internalNetworks;
    std::unordered_map<INetworkEntity::TConnectionId, PNetworkProvider> m_internalNetworksById;

    // service
    std::queue<PCommand> m_arrivedCommands;
    std::mutex m_mutexCommand;
    std::thread * m_threadNetworkCallbacks;

    // TODO: may slow down performance due to threads contending
    // boost::lockfree::spsc_queue<PCommand> m_arrivedCommandsLockFreeQueue;
};

#endif // COMMUNICATION_GATEWAY_H

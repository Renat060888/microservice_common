#ifndef WEBSOCKET_SERVER_H
#define WEBSOCKET_SERVER_H

#ifndef Astra

#include <functional>

// TODO: implement also for secured connections ?
//#include <websocketpp/config/asio.hpp>

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

#include "communication/network_interface.h"

using TWebsocketppServer = websocketpp::server<websocketpp::config::asio>;

class WebsocketServer : public INetworkProvider, public INetworkClient
{
    friend void callbackIncomingMessage( WebsocketServer * _userData,
                                         websocketpp::connection_hdl _handler,
                                         TWebsocketppServer::message_ptr _message );
    friend void callbackOnOpenHandle( WebsocketServer * _userData, websocketpp::connection_hdl _handler );
    friend void callbackOnCloseHandle( WebsocketServer * _userData, websocketpp::connection_hdl _handler );
    friend void callbackOnFail( WebsocketServer * _userData, websocketpp::connection_hdl _handler );

    friend class WebsocketServerRequest;
public:
    struct SInitSettings {
        SInitSettings()
            : port(0)
            , serverPollTimeoutMillisec(100)
            , async(false)
        {}
        int port;
        int32_t serverPollTimeoutMillisec;
        bool async;

        std::function<INetworkEntity::TConnectionId()> generateConnectionId;
    };

    WebsocketServer( INetworkEntity::TConnectionId _id );
    ~WebsocketServer();

    bool init( SInitSettings _settings );

    // server interface
    virtual void addObserver( INetworkObserver * _observer ) override;
    virtual void runNetworkCallbacks() override;
    virtual void setPollTimeout( int32_t _timeoutMillsec ) override;

    // client interface
    virtual PEnvironmentRequest getRequestInstance() override;

private:
    virtual void shutdown() override;
    void sendAsyncRequest( const SNetworkPackage & _package, websocketpp::connection_hdl & _connectHandle );

    // data
    SInitSettings m_settings;
    std::vector<INetworkObserver *> m_observers;
    char * m_outcomingBuffer;
    int32_t m_outcomingBufferSize;
    std::atomic<bool> m_connectionEstablished;
    std::map<std::string, INetworkEntity::TConnectionId> m_connectionsIds;
    int64_t m_connectionIdGenerator;
    // TODO: only from the first connection !
    websocketpp::connection_hdl m_handleOfFirstConnection;

    // service
    TWebsocketppServer m_websocketServer;
    std::mutex m_mutexSendProtection;
};
using PWebsocketServer = std::shared_ptr<WebsocketServer>;

#endif

#endif // WEBSOCKET_SERVER_H

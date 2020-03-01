#ifndef AMQP_CLIENT_C_H
#define AMQP_CLIENT_C_H

#include <string>
#include <vector>
#include <thread>
#include <map>
#include <atomic>
#include <queue>
#include <condition_variable>
#include <mutex>

#include <amqp.h>
#include <amqp_framing.h>
#include <amqp_tcp_socket.h>

#include "network_interface.h"

class AmqpClient : public INetworkProvider, public INetworkClient {
    friend class AmqpRequest;
public:    
    static constexpr int DEFAULT_AMQP_PORT = 5672;

    enum class EExchangeType {
        DIRECT      = 0x00,
        FANOUT      = 0x01,
        TOPIC       = 0x02,
        UNDEFINED   = 0xFF
    };

    struct SInitSettings {
        SInitSettings()
            : asyncMode(false)
            , port(DEFAULT_AMQP_PORT)
            , amqpVirtualHost("/")
            , login("guest")
            , pass("guest")
            , serverPollTimeoutMillisec(10)
            , deliveredMessageExpirationSec( 60 * 10 )
        {}

        bool asyncMode;
        std::string serverHost;
        int port;
        std::string amqpVirtualHost;
        std::string login;
        std::string pass;
        int64_t serverPollTimeoutMillisec;
        int32_t deliveredMessageExpirationSec;
    };

    AmqpClient( INetworkEntity::TConnectionId _id );
    ~AmqpClient();

    bool init( const SInitSettings & _params );
    const std::string & getLastError() { return m_lastError; }
    bool createExchangePoint( const std::string & _exchangePointName, EExchangeType _exchangePointType );
    bool createMailbox( const std::string & _exchangePointName, const std::string & _queueName, const std::string _bindingKeyName = "" );

    // server part
    virtual void setPollTimeout( int32_t _timeoutMillsec ) override;
    virtual void addObserver( INetworkObserver * _observer ) override;
    virtual void runNetworkCallbacks() override;

    // client part
    virtual PEnvironmentRequest getRequestInstance() override;


private:
    virtual void shutdown() override;

    // sync
    std::string sendPackageBlocked( const std::string & _msg,
                                    const std::string & _corrId,
                                    const std::string & _exchangeName,
                                    const std::string & _routingName );

    // async
    bool checkResponseReadyness( const std::string & _corrId );
    std::string getAsyncResponse( const std::string & _corrId );
    bool sendPackageAsync( const std::string & _msg,
                           const std::string & _corrId,
                           const std::string & _exchangeName,
                           const std::string & _routingName );

    void threadReceiveLoop();
    void poll();

    bool initLowLevel( amqp_connection_state_t & _connection, const SInitSettings & _params );

    // data
    std::string m_msgExpirationMillisecStr;
    std::vector<INetworkObserver *> m_observers;
    std::atomic_bool m_shutdownCalled;
    SInitSettings m_settings;
    std::string m_lastError;
    std::map<TCorrelationId, std::string> m_readyResponsesToAsyncMessages;

    // service
    amqp_connection_state_t m_connTransmit;
    amqp_connection_state_t m_connReceive;
    std::thread * m_threadIncomingPackages;
    std::mutex m_muSendBlocked;
    std::mutex m_muSendAsync;
};
using PAmqpClient = std::shared_ptr<AmqpClient>;

#endif // AMQP_CLIENT_C_H




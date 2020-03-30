#ifndef AMQP_CLIENT_C_H
#define AMQP_CLIENT_C_H

#include <string>
#include <vector>
#include <thread>
#include <map>
#include <set>
#include <atomic>
#include <queue>
#include <condition_variable>
#include <mutex>

#include <amqp.h>
#include <amqp_framing.h>
#include <amqp_tcp_socket.h>

#include "network_interface.h"

class AmqpClient : public INetworkProvider, public INetworkClient
{
    friend class AmqpRequest;
    static constexpr int DEFAULT_AMQP_PORT = 5672;
public:    
    static constexpr const char REPLY_TO_DELIMETER = '%';

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
            , deliveredMessageExpirationSec(30)
            , syncRequestTimeoutMillisec(3000)
        {}

        bool asyncMode;
        std::string serverHost;
        int port;
        std::string amqpVirtualHost;
        std::string login;
        std::string pass;
        int64_t serverPollTimeoutMillisec;
        int32_t deliveredMessageExpirationSec;
        int64_t syncRequestTimeoutMillisec;
    };

    struct SState {
        SInitSettings settings;
        std::string m_lastError;
    };

    AmqpClient( INetworkEntity::TConnectionId _id );
    ~AmqpClient();

    bool init( const SInitSettings & _params );
    const SState & getState() { return m_state; }
    bool createExchangePoint( const std::string & _exchangePointName, EExchangeType _exchangePointType );
    bool createMailbox( const std::string & _exchangePointName,
                        const std::string & _queueName,
                        const std::string _bindingKeyName = "",
                        bool _startConsume = true );

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
                                    const std::string & _routingName,
                                    const std::string & _replyTo );

    // async
    bool checkResponseReadyness( const std::string & _corrId );
    std::string getAsyncResponse( const std::string & _corrId );
    void refuseFromResponse( const std::string & _corrId );
    bool sendPackageAsync( const std::string & _msg,
                           const std::string & _corrId,
                           const std::string & _exchangeName,
                           const std::string & _routingName,
                           const std::string & _replyTo );

    void threadReceiveLoop();
    void poll();

    bool initLowLevel( amqp_connection_state_t & _connection, const SInitSettings & _params );

    // data
    std::string m_msgExpirationMillisecStr;
    std::vector<INetworkObserver *> m_observers;
    std::atomic_bool m_shutdownCalled;
    std::map<TCorrelationId, std::string> m_readyResponsesToAsyncMessages;
    std::set<TCorrelationId> m_refusedMessages;
    SState m_state;
    TCorrelationId m_syncRequestCorrelationId;
    bool m_syncRequestPerformed;

    // service
    amqp_connection_state_t m_connTransmit;
    amqp_connection_state_t m_connReceive;
    std::thread * m_threadIncomingPackages;
    std::mutex m_muSendBlocked;
    std::mutex m_muSendAsync;
    std::condition_variable m_cvResponseToBlockedRequestArrived;
};
using PAmqpClient = std::shared_ptr<AmqpClient>;

#endif // AMQP_CLIENT_C_H




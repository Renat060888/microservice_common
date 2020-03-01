#ifndef SHELL_H
#define SHELL_H

#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <functional>

#include "communication/network_interface.h"

class Shell : public INetworkProvider, public INetworkClient
{
    friend class ShellRequest;
public:
    enum class EShellMode {
        CLIENT,
        SERVER,
        UNDEFINED
    };

    enum class EMessageMode {
        WITHOUT_SIZE,
        WITH_SIZE,
        UNDEFINED
    };

    struct SInitSettings {
        SInitSettings()
            : shellMode(EShellMode::UNDEFINED)
            , serverPollTimeoutMillisec(500) // TODO: чем меньше, тем больше вероятность что придет мусорный messageSize
            , asyncServerMode(false)
            , asyncClientModeRequests(false)
            , messageMode(EMessageMode::UNDEFINED)
        {}
        EShellMode shellMode;
        int64_t serverPollTimeoutMillisec;
        bool asyncServerMode;
        std::string socketFileName;
        bool asyncClientModeRequests;
        EMessageMode messageMode;
    };

    Shell( INetworkEntity::TConnectionId _id );
    virtual ~Shell();

    bool init( SInitSettings _settings );
    virtual bool isConnectionEstablished() override;

    // server side
    virtual void runNetworkCallbacks() override;
    virtual void setPollTimeout( int32_t _timeoutMillsec ) override;
    void addObserver( INetworkObserver * _observer ) override;
    void removeObserver( INetworkObserver * _observer ) override;

    // client side
    virtual PEnvironmentRequest getRequestInstance() override;
    std::string makeBlockedRequest( std::string _msg );


private:
    virtual void shutdown() override;

    void threadClientAccepting();
    void threadAsyncClientModeRequests();
    void threadAsyncClientModeRequestsWithSize();
    void threadSecondTry();

    bool initClient();
    bool initServer();

    void sendWithoutSize( int _socketDescr, std::string _msg );
    void sendWithSize( int _socketDescr, std::string _msg );
    std::string receiveWithoutSize( int _socketDescr );
    std::string receiveWithSize( int _socketDescr );

    // data
    std::vector<INetworkObserver *> m_observers;
    int m_clientSocketDscr;
    int m_serverSocketDscr;
    bool m_connectionEstablished;
    bool m_shutdownCalled;
    SInitSettings m_settings;

    // service
    std::function<void(int,std::string)> m_sendProxy;
    std::function<std::string(int)> m_receiveProxy;
    std::thread * m_threadClientAccepting;
    std::thread * m_threadAsyncClientMode;
    std::mutex m_observerLock;

    struct SPrivateImpl * m_privateImpl;
};
using PShell = std::shared_ptr<Shell>;

#endif // SHELL_H

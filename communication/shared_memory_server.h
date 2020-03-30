#ifndef SHARED_MEMORY_SERVER_H
#define SHARED_MEMORY_SERVER_H

#include "communication/network_interface.h"

class SharedMemoryServer : public INetworkProvider
{
public:
    SharedMemoryServer( INetworkEntity::TConnectionId _id );

    virtual void shutdown() override;
    virtual void runNetworkCallbacks() override;
    virtual void addObserver( INetworkObserver * _observer ) override;
    virtual void setPollTimeout( int32_t _timeoutMillsec ) override;

private:


};

#endif // SHARED_MEMORY_SERVER_H

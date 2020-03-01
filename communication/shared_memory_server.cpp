
#include "shared_memory_server.h"

using namespace std;

SharedMemoryServer::SharedMemoryServer( INetworkEntity::TConnectionId _id )
    : INetworkProvider(_id)
{
    // TODO: current solution is Gstreamer SHMSRC
}

void SharedMemoryServer::shutdown(){

}

void SharedMemoryServer::runNetworkCallbacks(){

}

void SharedMemoryServer::addObserver( INetworkObserver * _observer ){

}

void SharedMemoryServer::setPollTimeout( int32_t _timeoutMillsec ){

}

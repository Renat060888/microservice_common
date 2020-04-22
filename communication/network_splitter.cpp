
#include "network_splitter.h"

using namespace std;

NetworkSplitter::NetworkSplitter( INetworkEntity::TConnectionId _id )
    : INetworkProvider(_id)
    , INetworkClient(_id)
{

}

bool NetworkSplitter::init( SInitSettings _settings ){

    m_settings = _settings;


    return true;
}

void NetworkSplitter::addObserver( INetworkObserver * _observer ){

    PNetworkProvider provider = std::dynamic_pointer_cast<INetworkProvider>( m_settings.m_realCommunicator );
    provider->addObserver( _observer );
}

void NetworkSplitter::removeObserver( INetworkObserver * _observer ){

    PNetworkProvider provider = std::dynamic_pointer_cast<INetworkProvider>( m_settings.m_realCommunicator );
    provider->removeObserver( _observer );
}

// server interface
void NetworkSplitter::runNetworkCallbacks(){
    // dummy
}

void NetworkSplitter::setPollTimeout( int32_t _timeoutMillsec ){
    // dummy
}

// client interface
PEnvironmentRequest NetworkSplitter::getRequestInstance(){
    return m_settings.m_realCommunicator->getRequestInstance();
}

void NetworkSplitter::shutdown(){
    // dummy
}

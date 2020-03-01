
#include "amqp_client_c.h"
#include "i_amqp_controller.h"

using namespace std;

AmqpController::AmqpController( INetworkEntity::TConnectionId _id )
    : INetworkClient(_id)
    , INetworkProvider(_id)
{

}

AmqpController::~AmqpController()
{

}

bool AmqpController::init( const SInitSettings & _settings ){

    m_state.settings = _settings;

    if( ! configureRoute(_settings) ){
        return false;
    }


    return true;
}

bool AmqpController::configureRoute( const SInitSettings & _settings ){

    PAmqpClient originalClient = std::dynamic_pointer_cast<AmqpClient>( _settings.client );


    // declare a MAILBOX from which messages will be RECEIVED
    const bool rt3 = originalClient->createExchangePoint( _settings.route.predatorExchangePointName,
                                                          AmqpClient::EExchangeType::DIRECT );
    if( ! rt3 ){
        m_state.lastError = originalClient->getLastError();
        return false;
    }

    const bool rt4 = originalClient->createMailbox( _settings.route.predatorExchangePointName,
                                                    _settings.route.predatorQueueName,
                                                    _settings.route.predatorRoutingKeyName );
    if( ! rt4 ){
        m_state.lastError = originalClient->getLastError();
        return false;
    }


    // declare an EXCHANGE to which messages will be SENT ( otherwise crash while sending ? )
    if( ! _settings.route.targetExchangePointName.empty() ){

        const bool rt = originalClient->createExchangePoint( _settings.route.targetExchangePointName,
                                                             AmqpClient::EExchangeType::DIRECT );
        if( ! rt ){
            m_state.lastError = originalClient->getLastError();
            return false;
        }

        // TODO: does the sender have the right to create recepient 'mailbox' ?
        const bool rt2 = originalClient->createMailbox( _settings.route.targetExchangePointName,
                                                        _settings.route.targetQueueName,
                                                        _settings.route.targetRoutingKeyName );
        if( ! rt2 ){
            m_state.lastError = originalClient->getLastError();
            return false;
        }
    }

    return true;
}

PEnvironmentRequest AmqpController::getRequestInstance(){

    PEnvironmentRequest request = AmqpController::getRequestInstance();
    request->setUserData( & m_state.settings.route );
    return request;
}

void AmqpController::shutdown(){

}

void AmqpController::runNetworkCallbacks(){

}

void AmqpController::setPollTimeout( int32_t _timeoutMillsec ){

}

void AmqpController::addObserver( INetworkObserver * _observer ){

    PNetworkProvider provider = std::dynamic_pointer_cast<INetworkProvider>( m_client );
    provider->addObserver( _observer );
}

void AmqpController::removeObserver( INetworkObserver * _observer ){

    PNetworkProvider provider = std::dynamic_pointer_cast<INetworkProvider>( m_client );
    provider->removeObserver( _observer );
}

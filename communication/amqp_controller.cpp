
#include "amqp_client_c.h"
#include "amqp_controller.h"

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

    if( _settings.route.predatorExchangePointName.find(AmqpClient::REPLY_TO_DELIMETER) != string::npos
        && _settings.route.predatorExchangePointName.find(AmqpClient::REPLY_TO_DELIMETER) != string::npos
        && _settings.route.predatorExchangePointName.find(AmqpClient::REPLY_TO_DELIMETER) != string::npos
        && _settings.route.predatorExchangePointName.find(AmqpClient::REPLY_TO_DELIMETER) != string::npos
        && _settings.route.predatorExchangePointName.find(AmqpClient::REPLY_TO_DELIMETER) != string::npos
        && _settings.route.predatorExchangePointName.find(AmqpClient::REPLY_TO_DELIMETER) != string::npos
            ){
        m_state.lastError = string("symbol [") + AmqpClient::REPLY_TO_DELIMETER + string("] is prohibited in amqp routes");
        return false;
    }

    PAmqpClient originalClient = std::dynamic_pointer_cast<AmqpClient>( _settings.client );


    // declare a MAILBOX from which messages will be RECEIVED
    const bool rt3 = originalClient->createExchangePoint( _settings.route.predatorExchangePointName,
                                                          AmqpClient::EExchangeType::DIRECT );
    if( ! rt3 ){
        m_state.lastError = originalClient->getState().m_lastError;
        return false;
    }

    const bool rt4 = originalClient->createMailbox( _settings.route.predatorExchangePointName,
                                                    _settings.route.predatorQueueName,
                                                    _settings.route.predatorRoutingKeyName );
    if( ! rt4 ){
        m_state.lastError = originalClient->getState().m_lastError;
        return false;
    }


    // declare an EXCHANGE to which messages will be SENT ( otherwise crash while sending ? )
    if( ! _settings.route.targetExchangePointName.empty() ){

        const bool rt = originalClient->createExchangePoint( _settings.route.targetExchangePointName,
                                                             AmqpClient::EExchangeType::DIRECT );
        if( ! rt ){
            m_state.lastError = originalClient->getState().m_lastError;
            return false;
        }

        // NOTE: queue with routing key target must create itself
//        const bool rt4 = originalClient->createMailbox( _settings.route.targetExchangePointName,
//                                                        _settings.route.targetQueueName,
//                                                        _settings.route.targetRoutingKeyName,
//                                                        false );
//        if( ! rt4 ){
//            m_state.lastError = originalClient->getState().m_lastError;
//            return false;
//        }
    }

    return true;
}

PEnvironmentRequest AmqpController::getRequestInstance(){

    PEnvironmentRequest request = m_state.settings.client->getRequestInstance();
    request->setUserData( & m_state.settings.route );
    return request;
}

void AmqpController::shutdown(){

}

void AmqpController::runNetworkCallbacks(){

    // TODO: terrible solution
    PNetworkProvider provider = std::dynamic_pointer_cast<INetworkProvider>( m_state.settings.client );
    provider->runNetworkCallbacks();
}

void AmqpController::setPollTimeout( int32_t _timeoutMillsec ){

}

void AmqpController::addObserver( INetworkObserver * _observer ){

    PNetworkProvider provider = std::dynamic_pointer_cast<INetworkProvider>( m_state.settings.client );
    provider->addObserver( _observer );
}

void AmqpController::removeObserver( INetworkObserver * _observer ){

    PNetworkProvider provider = std::dynamic_pointer_cast<INetworkProvider>( m_state.settings.client );
    provider->removeObserver( _observer );
}

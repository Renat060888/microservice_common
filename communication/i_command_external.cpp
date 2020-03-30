
#include "i_command_external.h"

using namespace std;

ICommandExternal::ICommandExternal( common_types::SIncomingCommandGlobalServices * _services ) :
    ICommand(_services)
{

}


void ICommandExternal::sendResponse( const string & _outcomingMessage, void * _userData ){

    assert( m_request );

    // TODO: _userData - terrible solution at first look
    // Different network transports features problem, for example:
    // 1 codes for HTTP server
    // 2 route point & queue for AMQP server
    // 3 objrepr sensor id
    // 4 ?
    m_request->setUserData( _userData );    
    m_request->sendMessageAsync( _outcomingMessage, m_request->m_correlationId );
}

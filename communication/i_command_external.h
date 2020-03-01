#ifndef I_COMMAND_EXTERNAL_H
#define I_COMMAND_EXTERNAL_H

#include "i_command.h"

class ICommandExternal : public ICommand
{
public:
    ICommandExternal( common_types::SIncomingCommandGlobalServices * _services );


protected:
    void sendResponse( const std::string & _outcomingMessage, void * _userData = nullptr );


private:

};

#endif // I_COMMAND_EXTERNAL_H

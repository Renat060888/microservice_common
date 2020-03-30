#ifndef I_COMMAND_FACTORY_H
#define I_COMMAND_FACTORY_H

#include "i_command.h"
#include "network_interface.h"

class ICommandFactory
{
public:
    ICommandFactory();
    virtual ~ICommandFactory(){}

    virtual PCommand createCommand( PEnvironmentRequest _request ) = 0;
};

#endif // COMMAND_FACTORY_H

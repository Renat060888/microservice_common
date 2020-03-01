#ifndef I_COMMAND_H
#define I_COMMAND_H

#include <memory>

#include "network_interface.h"
#include "common/ms_common_types.h"

class ICommand
{
public:
    ICommand( common_types::SIncomingCommandGlobalServices * _services );
    virtual ~ICommand();

    virtual bool exec() = 0;

    PEnvironmentRequest m_request;


protected:    
    common_types::SIncomingCommandGlobalServices * m_services;



};
using PCommand = std::shared_ptr<ICommand>;

#endif // I_COMMAND_H

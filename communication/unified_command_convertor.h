#ifndef SOURCE_PARSER_H
#define SOURCE_PARSER_H

#include <string>
#include <map>

#include "common/ms_common_types.h"

class UnifiedCommandConvertor
{
public:
    UnifiedCommandConvertor();
    virtual ~UnifiedCommandConvertor();

    virtual std::string getCommandsFromProgramArgs( const std::map<common_types::TCommandLineArgKey, common_types::TCommandLineArgVal> & _args ) = 0;
    virtual std::string getCommandsFromConfigFile( const std::string & _commands ) = 0;
    virtual std::string getCommandsFromHTTPRequest( const std::string & _httpMethod,
                                                    const std::string & _uri,
                                                    const std::string & _queryString,
                                                    const std::string & _body ) = 0;

private:



};

#endif // SOURCE_PARSER_H

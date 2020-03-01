#ifndef A_ARGS_PARSER_H
#define A_ARGS_PARSER_H

#include <unordered_map>

#include <boost/program_options.hpp>

#include "communication/unified_command_convertor.h"
#include "common/ms_common_utils.h"

namespace bpo = boost::program_options;

template< typename T_Keys >
class AArgsParser
{
    static constexpr const char * PRINT_HEADER = "ArgsParser:";
public:
    struct SInitSettings {
        SInitSettings()            
            : argc(0)
            , argv(nullptr)
            , commandConvertor(nullptr)
        {}        
        int argc;
        char ** argv;
        UnifiedCommandConvertor * commandConvertor;
        std::function<void()> printConfigExample;
    };

    bool init( const SInitSettings & _settings ){
        assert( _settings.commandConvertor );

        m_settings = _settings;

        fillArgumentsTemplateMethod();

        return true;
    }

    const SInitSettings & getSettings(){ return m_settings; }

    const std::string getVal( T_Keys _key ){
        auto iter = m_commmandLineArgs.find( _key );
        if( iter != m_commmandLineArgs.end() ){
            return iter->second;
        }

        PRELOG_ERR << PRINT_HEADER << " WARN - such arg not found, enum number [" << (int)_key << "]" << std::endl;
        return std::string();
    }


protected:
    virtual bpo::options_description getArgumentsDescrTemplateMethodPart() = 0;
    virtual void checkArgumentsTemplateMethodPart( const bpo::variables_map & _varMap ) = 0;
    virtual void version() = 0;
    virtual void about() = 0;

    AArgsParser(){}
    virtual ~AArgsParser(){}

    AArgsParser( const AArgsParser & _inst ) = delete;
    AArgsParser & operator=( const AArgsParser & _inst ) = delete;

    std::unordered_map<T_Keys, std::string> m_commmandLineArgs;


private:
    void fillArgumentsTemplateMethod(){
        // arguments are descrybed in derive class
        bpo::options_description desc = getArgumentsDescrTemplateMethodPart();

        // try to parse
        bpo::variables_map varMap;

        try{
            bpo::parsed_options parsed = bpo::command_line_parser( m_settings.argc, m_settings.argv ).options(desc).run();
            bpo::store( parsed, varMap );
            bpo::notify( varMap );
        }
        catch( std::exception & _ex ){
            PRELOG_ERR << "ERROR: " << _ex.what() << std::endl;
            PRELOG_ERR << desc << std::endl;
            ::exit( EXIT_FAILURE );
        }

        // default arguments
        if( varMap.empty() ){
            PRELOG_ERR << "ERROR: no arguments provided" << std::endl;
            PRELOG_ERR << desc << std::endl;
            ::exit( EXIT_FAILURE );
        }

        if( varMap.find("print-main-config") != varMap.end() ){
            m_settings.printConfigExample();
            ::exit( EXIT_SUCCESS );
        }
        if( varMap.find("version") != varMap.end() ){
            version();
            ::exit( EXIT_SUCCESS );
        }
        if( varMap.find("about") != varMap.end() ){
            about();
            ::exit( EXIT_SUCCESS );
        }
        if( varMap.find("help") != varMap.end() ){
            PRELOG_INFO << desc << std::endl;
            ::exit( EXIT_SUCCESS );
        }

        // specific args for project
        checkArgumentsTemplateMethodPart( varMap );
    }

    // data
    SInitSettings m_settings;    

    // service


};

#endif // A_ARGS_PARSER_H

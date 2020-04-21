#ifndef OBJREPR_BUS_H
#define OBJREPR_BUS_H

//#define OBJREPR_LIBRARY_EXIST

#include <string>
#include <future>

#include "common/ms_common_types.h"

class ObjreprBus : public common_types::IContextService
{
    static bool m_systemInited;
public:
    struct SInitSettings {
        std::string objreprConfigPath;
        std::string initialContextName;
    };

    // TODO: do instead 'getLastError()'
    struct SState {
        SInitSettings settings;
        std::string lastError;
    };

    static ObjreprBus & singleton(){
        static ObjreprBus instance;
        return instance;
    }

    bool init( const SInitSettings & _settings );
    void shutdown();
    const std::string & getLastError(){ return m_lastError; }

    virtual bool openContext( common_types::TContextId _ctxId ) override;
    virtual bool openContextAsync( common_types::TContextId _ctxId ) override;
    virtual bool closeContext() override;

    virtual common_types::TContextId getCurrentContextId() override;
    virtual common_types::TContextId getContextIdByName( const std::string & _ctxName ) override;
    virtual std::string getContextNameById( common_types::TContextId _ctxId ) override;


protected:
    ObjreprBus();
    virtual ~ObjreprBus();

    virtual void shutdownDerive(){}

    // NOTE: stuff for derived classes ( video-server-object, player-object, etc... )


private:
    ObjreprBus( const ObjreprBus & _inst ) = delete;
    ObjreprBus & operator=( const ObjreprBus & _inst ) = delete;

    void threadObjreprContextLoading( common_types::TContextId _ctxId );

    bool launch( const std::string & _configPath );    



    // data    
    std::string m_lastError;

    // service
    std::future<void> m_futureObjreprContextLoading;



};

#endif // OBJREPR_BUS_H

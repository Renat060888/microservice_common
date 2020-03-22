#ifndef OBJREPR_BUS_H
#define OBJREPR_BUS_H

//#define OBJREPR_LIBRARY_EXIST

#include <string>
#include <future>

#include <microservice_common/common/ms_common_types.h>

class ObjreprBus
{
    static bool m_systemInited;
public:
    static ObjreprBus & singleton(){
        static ObjreprBus instance;
        return instance;
    }

    bool init();
    void shutdown();
    const std::string & getLastError(){ return m_lastError; }

    bool openContextAsync( common_types::TContextId _ctxId );
    bool closeContext();
    common_types::TContextId getCurrentContextId();


private:
    ObjreprBus();
    ~ObjreprBus();

    ObjreprBus( const ObjreprBus & _inst ) = delete;
    ObjreprBus & operator=( const ObjreprBus & _inst ) = delete;

    void threadObjreprContextLoading( common_types::TContextId _ctxId );

    bool launch( const std::string & _configPath );    
    bool openContext( const std::string & _ctxName );

    common_types::TContextId getContextIdByName( const std::string & _ctxName );
    std::string getContextNameById( common_types::TContextId _ctxId );


    // data
    std::string m_lastError;

    // service
    std::future<void> m_futureObjreprContextLoading;



};
#define OBJREPR_BUS ObjreprBus::singleton()

#endif // OBJREPR_BUS_H

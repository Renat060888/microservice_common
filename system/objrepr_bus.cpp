
#ifdef OBJREPR_LIBRARY_EXIST
#include <objrepr/reprServer.h>
#endif

#include <microservice_common/common/ms_common_utils.h>
#include <microservice_common/system/logger.h>

#include "common/common_vars.h"
#include "config_reader.h"
#include "objrepr_bus.h"

using namespace std;

bool ObjreprBus::m_systemInited = false;

ObjreprBus::ObjreprBus()
{

}

ObjreprBus::~ObjreprBus()
{
    shutdown();
}

bool ObjreprBus::init(  ){
#ifdef OBJREPR_LIBRARY_EXIST
    if( m_systemInited ){
        return true;
    }

    VS_LOG_INFO << "objrepr system-init begin" << endl;

    if( ! launch(CONFIG_PARAMS.OBJREPR_CONFIG_PATH) ){
        assert( false && "objrepr launch crashed :(" );
    }

    if( ! CONFIG_PARAMS.OBJREPR_INITIAL_CONTEXT_NAME.empty() ){

        if( ! openContext(ConfigReader::singleton().get().OBJREPR_INITIAL_CONTEXT_NAME) ){
            assert( false && "objrepr opening context crashed :(" );
        }        
    }

    VS_LOG_INFO << "objrepr system-init success" << endl;
#endif
    m_systemInited = true;
    return true;
}

void ObjreprBus::shutdown(){
#ifdef OBJREPR_LIBRARY_EXIST
    if( m_systemInited ){
        assert( objrepr::RepresentationServer::instance()->state() != objrepr::RepresentationServer::State::ST_Stopped );

        if( ! m_videoServerMirrorName.empty() ){
            objrepr::SpatialObjectPtr videoServer = getThisVideoServerObject( CONFIG_PARAMS.OBJREPR_GDM_VIDEO_CONTAINER_CLASSINFO_NAME,
                                                                              CONFIG_PARAMS.SYSTEM_UNIQUE_ID );
            setOnline( videoServer, false );
        }

        m_systemInited = false;
    }
#endif
}

bool ObjreprBus::launch( const std::string & _configPath ){
#ifdef OBJREPR_LIBRARY_EXIST
    const bool configured = objrepr::RepresentationServer::instance()->configure( _configPath.c_str() );
    if( ! configured ){
        VS_LOG_CRITICAL << "objrepr Can't configure by: [1], reason: [2] " << _configPath << " " << objrepr::RepresentationServer::instance()->errString() << endl;
        return false;
    }

    const bool launched = objrepr::RepresentationServer::instance()->launch();
    if( ! launched ){
        VS_LOG_CRITICAL << "objrepr Can't launch, reason: " << objrepr::RepresentationServer::instance()->errString() << endl;
        return false;
    }
#endif
    return true;
}

bool ObjreprBus::openContext( const std::string & _ctxName ){
#ifdef OBJREPR_LIBRARY_EXIST
    const bool opened = objrepr::RepresentationServer::instance()->setCurrentContext( _ctxName.c_str() );
    if( ! opened ){
        m_lastError = objrepr::RepresentationServer::instance()->errString();
        VS_LOG_CRITICAL << "objrepr context open fail, reason: " << objrepr::RepresentationServer::instance()->errString() << endl;
        return false;
    }

    VS_LOG_INFO << "================ OBJREPR CONTEXT OPENED: [" << objrepr::RepresentationServer::instance()->currentContext()->name()
             << "] ================"
             << endl;
#endif
    return true;
}

bool ObjreprBus::openContextAsync( common_types::TContextId _ctxId ){

    m_futureObjreprContextLoading = std::async( std::launch::async, & ObjreprBus::threadObjreprContextLoading, this, _ctxId );
    return true;
}

common_types::TContextId ObjreprBus::getCurrentContextId(){
#ifdef OBJREPR_LIBRARY_EXIST
    return objrepr::RepresentationServer::instance()->currentContext()->id();
#endif
}

bool ObjreprBus::closeContext(){
#ifdef OBJREPR_LIBRARY_EXIST
    if( ! objrepr::RepresentationServer::instance()->currentContext() ){
        stringstream ss;
        ss << "objrepr context open fail, nothing to close";
        m_lastError = ss.str();
        VS_LOG_CRITICAL << ss.str() << endl;
        return false;
    }

    const bool opened = objrepr::RepresentationServer::instance()->setCurrentContext( (uint32_t)0 );
    assert( opened );
#endif
    return true;
}

common_types::TContextId ObjreprBus::getContextIdByName( const std::string & _ctxName ){
#ifdef OBJREPR_LIBRARY_EXIST
    common_types::TContextId contextId = common_vars::INVALID_CONTEXT_ID;
    std::vector<objrepr::ContextPtr> contexts = objrepr::RepresentationServer::instance()->contextList();
    for( objrepr::ContextPtr & ctx : contexts ){
        if( ctx->name() == _ctxName ){
            contextId = ctx->id();
        }
    }

    return contextId;
#endif
}

std::string ObjreprBus::getContextNameById( common_types::TContextId _ctxId ){
#ifdef OBJREPR_LIBRARY_EXIST
    string contextName;
    std::vector<objrepr::ContextPtr> contexts = objrepr::RepresentationServer::instance()->contextList();
    for( objrepr::ContextPtr & ctx : contexts ){
        if( ctx->id() == _ctxId ){
            contextName = ctx->name();
        }
    }

    return contextName;
#endif
}

void ObjreprBus::threadObjreprContextLoading( common_types::TContextId _ctxId ){
#ifdef OBJREPR_LIBRARY_EXIST
    const bool opened = objrepr::RepresentationServer::instance()->setCurrentContext( _ctxId );
    if( ! opened ){
        m_lastError = objrepr::RepresentationServer::instance()->errString();
        VS_LOG_CRITICAL << "objrepr context open fail, reason: " << objrepr::RepresentationServer::instance()->errString() << endl;
    }
#endif
}








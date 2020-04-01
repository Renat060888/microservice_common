
#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>

#include "system/logger.h"
#include "common/ms_common_vars.h"
#include "common/ms_common_utils.h"
#include "database_manager_base.h"

using namespace std;
using namespace common_types;
using namespace common_vars;

static constexpr const char * PRINT_HEADER = "DatabaseMgr:";
static const string ARGS_DELIMETER = "$";

bool DatabaseManagerBase::m_systemInited = false;
int DatabaseManagerBase::m_instanceCounter = 0;
const std::string DatabaseManagerBase::ALL_CLIENT_OPERATIONS = "";
const common_types::TPid DatabaseManagerBase::ALL_PROCESS_EVENTS = 0;
const std::string DatabaseManagerBase::ALL_REGISTRATION_IDS = "";

DatabaseManagerBase::DatabaseManagerBase()
    : m_mongoClient(nullptr)
    , m_database(nullptr)
{

}

DatabaseManagerBase::~DatabaseManagerBase()
{    
    mongoc_cleanup();

    for( mongoc_collection_t * collect : m_allCollections ){
        mongoc_collection_destroy( collect );
    }
    mongoc_database_destroy( m_database );
    mongoc_client_destroy( m_mongoClient );
}

void DatabaseManagerBase::systemInit(){

    mongoc_init();    
    VS_LOG_INFO << PRINT_HEADER << " init success" << endl;
    m_systemInited = true;
}

// -------------------------------------------------------------------------------------
// service
// -------------------------------------------------------------------------------------
bool DatabaseManagerBase::init( SInitSettings _settings ){

    m_settings = _settings;

    // init mongo
    const mongoc_uri_t * uri = mongoc_uri_new_for_host_port( _settings.host.c_str(), _settings.port );
    if( ! uri ){
        VS_LOG_ERROR << PRINT_HEADER << " mongo URI creation failed by host: " << _settings.host << endl;
        return false;
    }

    m_mongoClient = mongoc_client_new_from_uri( uri );
    if( ! m_mongoClient ){
        VS_LOG_ERROR << PRINT_HEADER << " mongo connect failed to: " << _settings.host << endl;
        return false;
    }

    m_database = mongoc_client_get_database( m_mongoClient, _settings.databaseName.c_str() );

    m_tableWALClientOperations = mongoc_client_get_collection( m_mongoClient,
        _settings.databaseName.c_str(),
        (string("video_server_") + mongo_fields::wal_client_operations::COLLECTION_NAME).c_str() );

    m_tableWALProcessEvents = mongoc_client_get_collection( m_mongoClient,
        _settings.databaseName.c_str(),
        (string("video_server_") + mongo_fields::wal_process_events::COLLECTION_NAME).c_str() );

    m_allCollections.push_back( m_tableWALClientOperations );
    m_allCollections.push_back( m_tableWALProcessEvents );

    VS_LOG_INFO << PRINT_HEADER << " instance connected to [" << _settings.host << "]" << endl;
    return true;
}

inline bool DatabaseManagerBase::createIndex( const std::string & _tableName, const std::vector<std::string> & _fieldNames ){

    //
    bson_t keys;
    bson_init( & keys );

    for( const string & key : _fieldNames ){
        BSON_APPEND_INT32( & keys, key.c_str(), 1 );
    }

    //
    char * indexName = mongoc_collection_keys_to_index_string( & keys );
    bson_t * createIndex = BCON_NEW( "createIndexes",
                                     BCON_UTF8(_tableName.c_str()),
                                     "indexes", "[",
                                         "{", "key", BCON_DOCUMENT(& keys),
                                              "name", BCON_UTF8(indexName),
                                         "}",
                                     "]"
                                );

    //
    bson_t reply;
    bson_error_t error;
    const bool rt = mongoc_database_command_simple( m_database,
                                                    createIndex,
                                                    NULL,
                                                    & reply,
                                                    & error );


    if( ! rt ){
        VS_LOG_ERROR << PRINT_HEADER << " index creation failed, reason: " << error.message << endl;
        bson_destroy( createIndex );

        // TODO: remove later
        assert( rt );

        return false;
    }

    bson_destroy( createIndex );
    return false;
}

inline mongoc_collection_t * DatabaseManagerBase::getAnalyticContextTable( TPersistenceSetId _persId ){

    assert( _persId > 0 );

    mongoc_collection_t * contextTable = nullptr;
    auto iter = m_contextCollections.find( _persId );
    if( iter != m_contextCollections.end() ){
        contextTable = iter->second;
    }
    else{
        const string tableName = getTableName(_persId);
        contextTable = mongoc_client_get_collection(    m_mongoClient,
                                                        m_settings.databaseName.c_str(),
                                                        tableName.c_str() );

        createIndex( tableName, {mongo_fields::analytic::detected_object::SESSION,
                                 mongo_fields::analytic::detected_object::LOGIC_TIME}
                   );

        // TODO: add record to context info

        m_contextCollections.insert( {_persId, contextTable} );
    }

    return contextTable;
}

inline string DatabaseManagerBase::getTableName( common_types::TPersistenceSetId _persId ){

    const string name = string("video_server_") +
                        mongo_fields::analytic::COLLECTION_NAME +
                        "_" +
                        std::to_string(_persId);
//                        "_" +
//                        std::to_string(_sensorId);

    return name;
}

// -------------------------------------------------------------------------------------
// analytic events
// -------------------------------------------------------------------------------------
bool DatabaseManagerBase::writeTrajectoryData( TPersistenceSetId _persId, const vector<SPersistenceTrajectory> & _data ){

    //
    mongoc_collection_t * contextTable = getAnalyticContextTable( _persId );
    mongoc_bulk_operation_t * bulkedWrite = mongoc_collection_create_bulk_operation( contextTable, false, NULL );

    //
    for( const SPersistenceTrajectory & traj : _data ){

        bson_t * doc = BCON_NEW( mongo_fields::analytic::detected_object::OBJRERP_ID.c_str(), BCON_INT64( traj.objId ),
                                 mongo_fields::analytic::detected_object::STATE.c_str(), BCON_INT32( (int32_t)(traj.state) ),
                                 mongo_fields::analytic::detected_object::ASTRO_TIME.c_str(), BCON_INT64( 0 ),
                                 mongo_fields::analytic::detected_object::LOGIC_TIME.c_str(), BCON_INT64( traj.logicTime ),
                                 mongo_fields::analytic::detected_object::SESSION.c_str(), BCON_INT32( traj.session ),
                                 mongo_fields::analytic::detected_object::LAT.c_str(), BCON_DOUBLE( traj.latDeg ),
                                 mongo_fields::analytic::detected_object::LON.c_str(), BCON_DOUBLE( traj.lonDeg ),
                                 mongo_fields::analytic::detected_object::YAW.c_str(), BCON_DOUBLE( traj.heading )
                               );

        mongoc_bulk_operation_insert( bulkedWrite, doc );
        bson_destroy( doc );
    }

    //
    bson_error_t error;
    const bool rt = mongoc_bulk_operation_execute( bulkedWrite, NULL, & error );
    if( 0 == rt ){
        VS_LOG_ERROR << PRINT_HEADER << " bulked process event write failed, reason: " << error.message << endl;
        mongoc_bulk_operation_destroy( bulkedWrite );
        return false;
    }
    mongoc_bulk_operation_destroy( bulkedWrite );

    return true;
}

std::vector<SPersistenceTrajectory> DatabaseManagerBase::readTrajectoryData( const SPersistenceSetFilter & _filter ){

    mongoc_collection_t * contextTable = getAnalyticContextTable( _filter.persistenceSetId );
    assert( contextTable );

    bson_t * projection = nullptr;
    bson_t * query = nullptr;

    // TODO: clarify load mode - only one step, steps range, whole data

    if( _filter.minLogicStep == _filter.maxLogicStep ){
        query = BCON_NEW( "$and", "[", "{", mongo_fields::analytic::detected_object::SESSION.c_str(), BCON_INT32(_filter.sessionId), "}",
                                       "{", mongo_fields::analytic::detected_object::LOGIC_TIME.c_str(), "{", "$eq", BCON_INT64(_filter.minLogicStep), "}", "}",
                                  "]"
                        );
    }
    else if( _filter.minLogicStep != 0 || _filter.maxLogicStep != 0 ){
        query = BCON_NEW( "$and", "[", "{", mongo_fields::analytic::detected_object::SESSION.c_str(), BCON_INT32(_filter.sessionId), "}",
                                       "{", mongo_fields::analytic::detected_object::LOGIC_TIME.c_str(), "{", "$gte", BCON_INT64(_filter.minLogicStep), "}", "}",
                                       "{", mongo_fields::analytic::detected_object::LOGIC_TIME.c_str(), "{", "$lte", BCON_INT64(_filter.maxLogicStep), "}", "}",
                                  "]"
                        );
    }
    else{
        query = BCON_NEW( nullptr );
    }

    mongoc_cursor_t * cursor = mongoc_collection_find(  contextTable,
                                                        MONGOC_QUERY_NONE,
                                                        0,
                                                        0,
                                                        1000000, // 10000 ~= inf
                                                        query,
                                                        projection,
                                                        nullptr );

    std::vector<SPersistenceTrajectory> out;
    const uint32_t size = mongoc_cursor_get_batch_size( cursor );
    out.reserve( size );

    const bson_t * doc;
    while( mongoc_cursor_next( cursor, & doc ) ){
        bson_iter_t iter;

        SPersistenceTrajectory detectedObject;
        bson_iter_init_find( & iter, doc, mongo_fields::analytic::detected_object::OBJRERP_ID.c_str() );
        detectedObject.objId = bson_iter_int64( & iter );
        bson_iter_init_find( & iter, doc, mongo_fields::analytic::detected_object::ASTRO_TIME.c_str() );
        detectedObject.timestampMillisec = bson_iter_int64( & iter );
        bson_iter_init_find( & iter, doc, mongo_fields::analytic::detected_object::LOGIC_TIME.c_str() );
        detectedObject.logicTime = bson_iter_int64( & iter );
        bson_iter_init_find( & iter, doc, mongo_fields::analytic::detected_object::SESSION.c_str() );
        detectedObject.session = bson_iter_int32( & iter );
        bson_iter_init_find( & iter, doc, mongo_fields::analytic::detected_object::STATE.c_str() );
        detectedObject.state = (SPersistenceObj::EState)bson_iter_int32( & iter );

        if( detectedObject.state == SPersistenceObj::EState::ACTIVE ){
            bson_iter_init_find( & iter, doc, mongo_fields::analytic::detected_object::LAT.c_str() );
            detectedObject.latDeg = bson_iter_double( & iter );
            bson_iter_init_find( & iter, doc, mongo_fields::analytic::detected_object::LON.c_str() );
            detectedObject.lonDeg = bson_iter_double( & iter );
            bson_iter_init_find( & iter, doc, mongo_fields::analytic::detected_object::YAW.c_str() );
            detectedObject.heading = bson_iter_double( & iter );
        }

        out.push_back( detectedObject );
    }

    mongoc_cursor_destroy( cursor );
    bson_destroy( query );

    return out;
}

void DatabaseManagerBase::removeTotalData( const SPersistenceSetFilter & _filter ){

    // TODO: remove by logic step range

    mongoc_collection_t * contextTable = getAnalyticContextTable( _filter.persistenceSetId );
    assert( contextTable );

    bson_t * query = BCON_NEW( nullptr );

    const bool result = mongoc_collection_remove( contextTable, MONGOC_REMOVE_NONE, query, nullptr, nullptr );

    bson_destroy( query );
}

bool DatabaseManagerBase::writeWeatherData( TPersistenceSetId _persId, const std::vector<SPersistenceWeather> & _data ){

    assert( false && "TODO: do" );

    return true;
}

std::vector<common_types::SPersistenceWeather> DatabaseManagerBase::readWeatherData( const common_types::SPersistenceSetFilter & _filter ){

    std::vector<common_types::SPersistenceWeather> out;

    assert( false && "TODO: do" );

    return out;
}

TPersistenceSetId DatabaseManagerBase::writePersistenceSetMetadata( const common_types::SPersistenceMetadataVideo & _type ){

    // TODO: implement me
}

TPersistenceSetId DatabaseManagerBase::writePersistenceSetMetadata( const common_types::SPersistenceMetadataDSS & _type ){

    // TODO: implement me
}

TPersistenceSetId DatabaseManagerBase::writePersistenceSetMetadata( const common_types::SPersistenceMetadataRaw & _type ){

    // TODO: implement me
}

SPersistenceMetadata DatabaseManagerBase::getPersistenceSetMetadata( common_types::TContextId _ctxId ){

    // TODO: implement me
}

void DatabaseManagerBase::removePersistenceSetMetadata( common_types::TPersistenceSetId _id ){

    // TODO: implement me
}

std::vector<SEventsSessionInfo> DatabaseManagerBase::getPersistenceSetSessions( TPersistenceSetId _persId ){

    bson_t * cmd = BCON_NEW(    "distinct", BCON_UTF8( getTableName(_persId).c_str() ),
                                "key", BCON_UTF8( mongo_fields::analytic::detected_object::SESSION.c_str() ),
                                "$sort", "{", "logic_time", BCON_INT32(-1), "}"
                            );

    bson_t reply;
    bson_error_t error;
    const bool rt = mongoc_database_command_simple( m_database,
                                                    cmd,
                                                    NULL,
                                                    & reply,
                                                    & error );

    // fill array with session numbers
    bson_iter_t iter;
    bson_iter_t arrayIter;

    if( ! (bson_iter_init_find( & iter, & reply, "values")
            && BSON_ITER_HOLDS_ARRAY( & iter )
            && bson_iter_recurse( & iter, & arrayIter ))
      ){
        VS_LOG_ERROR << PRINT_HEADER << "TODO: print" << endl;
        return std::vector<SEventsSessionInfo>();
    }

    // get info about each session
    std::vector<SEventsSessionInfo> out;

    while( bson_iter_next( & arrayIter ) ){
        if( BSON_ITER_HOLDS_INT32( & arrayIter ) ){
            const TSessionNum sessionNumber = bson_iter_int32( & arrayIter );

            SEventsSessionInfo info;
            info.number = sessionNumber;
            info.steps = getSessionSteps( _persId, sessionNumber );
            info.minLogicStep = info.steps.front().logicStep;
            info.maxLogicStep = info.steps.back().logicStep;
            info.minTimestampMillisec = info.steps.front().timestampMillisec;
            info.maxTimestampMillisec = info.steps.back().timestampMillisec;

            out.push_back( info );
        }
    }

    bson_destroy( cmd );
    bson_destroy( & reply );
    return out;
}

std::vector<SObjectStep> DatabaseManagerBase::getSessionSteps( TPersistenceSetId _persId, TSessionNum _sesNum ){

    mongoc_collection_t * contextTable = getAnalyticContextTable( _persId );
    assert( contextTable );

    bson_t * pipeline = BCON_NEW( "pipeline", "[",
                                  "{", "$match", "{", "session", "{", "$eq", BCON_INT32(_sesNum), "}", "}", "}",
                                  "{", "$group", "{", "_id", "$logic_time", "maxAstroTime", "{", "$max", "$astro_time", "}", "}", "}",
                                  "{", "$project", "{", "_id", BCON_INT32(1), "maxAstroTime", BCON_INT32(1), "}", "}",
                                  "{", "$sort", "{", "_id", BCON_INT32(1), "}", "}",
                                  "]"
                                );

    mongoc_cursor_t * cursor = mongoc_collection_aggregate( contextTable,
                                                            MONGOC_QUERY_NONE,
                                                            pipeline,
                                                            nullptr,
                                                            nullptr );
    std::vector<SObjectStep> out;

    const bson_t * doc;
    while( mongoc_cursor_next( cursor, & doc ) ){
        bson_iter_t iter;

        SObjectStep step;

        bson_iter_init_find( & iter, doc, "_id" );
        step.logicStep = bson_iter_int64( & iter );

        bson_iter_init_find( & iter, doc, "maxAstroTime" );
        step.timestampMillisec = bson_iter_int64( & iter );

        out.push_back( step );
    }

    bson_destroy( pipeline );
    mongoc_cursor_destroy( cursor );

    return out;
}

// -------------------------------------------------------------------------------------
// WAL
// -------------------------------------------------------------------------------------
bool DatabaseManagerBase::writeClientOperation( const common_types::SWALClientOperation & _operation ){

    bson_t * query = BCON_NEW( mongo_fields::wal_client_operations::UNIQUE_KEY.c_str(), BCON_UTF8( _operation.uniqueKey.c_str() ) );
    bson_t * update = BCON_NEW( "$set", "{",
                             mongo_fields::wal_client_operations::BEGIN.c_str(), BCON_BOOL( _operation.begin ),
                             mongo_fields::wal_client_operations::UNIQUE_KEY.c_str(), BCON_UTF8( _operation.uniqueKey.c_str() ),
                             mongo_fields::wal_client_operations::FULL_TEXT.c_str(), BCON_UTF8( _operation.commandFullText.c_str() ),
                            "}" );

    const bool rt = mongoc_collection_update( m_tableWALClientOperations,
                                  MONGOC_UPDATE_UPSERT,
                                  query,
                                  update,
                                  NULL,
                                  NULL );

    if( ! rt ){
        VS_LOG_ERROR << "client operation write failed" << endl;
        bson_destroy( query );
        bson_destroy( update );
        return false;
    }

    bson_destroy( query );
    bson_destroy( update );
    return true;
}

std::vector<common_types::SWALClientOperation> DatabaseManagerBase::getClientOperations(){

    // NOTE: at this moment w/o filter. Two advantages:
    // - simple & stable database query
    // - fast filter criteria changing on client side
    bson_t * query = BCON_NEW( nullptr );

    mongoc_cursor_t * cursor = mongoc_collection_find(  m_tableWALClientOperations,
                                                        MONGOC_QUERY_NONE,
                                                        0,
                                                        0,
                                                        1000000, // 10000 ~= inf
                                                        query,
                                                        nullptr,
                                                        nullptr );

    std::vector<common_types::SWALClientOperation> out;
    const bson_t * doc;
    while( mongoc_cursor_next( cursor, & doc ) ){

        uint len;
        bson_iter_t iter;

        common_types::SWALClientOperation oper;
        bson_iter_init_find( & iter, doc, mongo_fields::wal_client_operations::BEGIN.c_str() );
        oper.begin = bson_iter_bool( & iter );        
        bson_iter_init_find( & iter, doc, mongo_fields::wal_client_operations::UNIQUE_KEY.c_str() );
        oper.uniqueKey = bson_iter_utf8( & iter, & len );
        bson_iter_init_find( & iter, doc, mongo_fields::wal_client_operations::FULL_TEXT.c_str() );
        oper.commandFullText = bson_iter_utf8( & iter, & len );

        out.push_back( oper );
    }

    mongoc_cursor_destroy( cursor );
    bson_destroy( query );

    return out;
}

std::vector<common_types::SWALClientOperation> DatabaseManagerBase::getNonIntegrityClientOperations(){

    // TODO: join this 2 queries
    // WTF 1: BCON_INT32(1) != "1"
    // WTF 2: "0" in BSON_APPEND_UTF8( & arrayElement, "0", criteriaElement.c_str() )

    // find single UKeys
    std::vector<string> singleUniqueKeys;
    {
        bson_t * pipeline = BCON_NEW( "pipeline", "[",
                                      "{", "$group", "{", "_id", "$unique_key", "count", "{", "$sum", BCON_INT32(1), "}", "}", "}",
                                      "{", "$match", "{", "count", "{", "$eq", BCON_INT32(1), "}", "}", "}",
                                      "]"
                                    );

        mongoc_cursor_t * cursor = mongoc_collection_aggregate( m_tableWALClientOperations,
                                                                MONGOC_QUERY_NONE,
                                                                pipeline,
                                                                nullptr,
                                                                nullptr );
        const bson_t * doc;
        while( mongoc_cursor_next( cursor, & doc ) ){

            uint len;
            bson_iter_t iter;

            bson_iter_init_find( & iter, doc, "_id" );
            singleUniqueKeys.push_back( bson_iter_utf8( & iter, & len ) );
        }
    }

    // get info about this keys
    std::vector<common_types::SWALClientOperation> out;
    {
        bson_t criteria;
        bson_t arrayElement;
        bson_init( & criteria );
        bson_append_array_begin( & criteria, "$in", strlen("$in"), & arrayElement );
        for( const string & criteriaElement : singleUniqueKeys ){
            BSON_APPEND_UTF8( & arrayElement, "0", criteriaElement.c_str() );
        }
        bson_append_array_end( & criteria, & arrayElement );

        bson_t query;
        bson_init( & query );
        bson_append_document( & query, "unique_key", strlen("unique_key"), & criteria );

        mongoc_cursor_t * cursor = mongoc_collection_find(  m_tableWALClientOperations,
                                                            MONGOC_QUERY_NONE,
                                                            0,
                                                            0,
                                                            1000000, // 10000 ~= inf
                                                            & query,
                                                            nullptr,
                                                            nullptr );

        const bson_t * doc;
        while( mongoc_cursor_next( cursor, & doc ) ){

            uint len;
            bson_iter_t iter;

            common_types::SWALClientOperation oper;
            bson_iter_init_find( & iter, doc, mongo_fields::wal_client_operations::BEGIN.c_str() );
            oper.begin = bson_iter_bool( & iter );
            bson_iter_init_find( & iter, doc, mongo_fields::wal_client_operations::UNIQUE_KEY.c_str() );
            oper.uniqueKey = bson_iter_utf8( & iter, & len );
            bson_iter_init_find( & iter, doc, mongo_fields::wal_client_operations::FULL_TEXT.c_str() );
            oper.commandFullText = bson_iter_utf8( & iter, & len );

            assert( oper.begin );

            out.push_back( oper );
        }

        mongoc_cursor_destroy( cursor );
    }

    return out;
}

void DatabaseManagerBase::removeClientOperation( std::string _uniqueKey ){

    bson_t * query = nullptr;
    if( ALL_CLIENT_OPERATIONS == _uniqueKey ){
        query = BCON_NEW( nullptr );
    }
    else{
        query = BCON_NEW( mongo_fields::wal_client_operations::UNIQUE_KEY.c_str(), BCON_UTF8( _uniqueKey.c_str() ));
    }

    const bool result = mongoc_collection_remove( m_tableWALClientOperations, MONGOC_REMOVE_NONE, query, nullptr, nullptr );

    if( ! result ){
        // TODO: do
    }

    bson_destroy( query );
}

bool DatabaseManagerBase::writeProcessEvent( const common_types::SWALProcessEvent & _event, bool _launch ){

    bson_t * doc = BCON_NEW( mongo_fields::wal_process_events::PID.c_str(), BCON_INT32( _event.pid ),
                             mongo_fields::wal_process_events::LAUNCHED.c_str(), BCON_BOOL( _launch ),
                             mongo_fields::wal_process_events::PROGRAM_NAME.c_str(), BCON_UTF8( _event.programName.c_str() )
                           );

    // array
    bson_t arrayElement;
    bson_append_array_begin( doc, mongo_fields::wal_process_events::PROGRAM_ARGS.c_str(), strlen(mongo_fields::wal_process_events::PROGRAM_ARGS.c_str()), & arrayElement );
    for( const string & arg : _event.programArgs ){
        BSON_APPEND_UTF8( & arrayElement, "0", arg.c_str() );
    }
    bson_append_array_end( doc, & arrayElement );
    // array

    bson_error_t error;
    const bool rt = mongoc_collection_insert( m_tableWALProcessEvents,
                                              MONGOC_INSERT_NONE,
                                              doc,
                                              NULL,
                                              & error );

    if( 0 == rt ){
        VS_LOG_ERROR << PRINT_HEADER << " process event write failed, reason: " << error.message << endl;
        bson_destroy( doc );
        return false;
    }

    bson_destroy( doc );
    return true;
}

std::vector<common_types::SWALProcessEvent> DatabaseManagerBase::getProcessEvents( common_types::TPid _pid ){

    bson_t * query = nullptr;
    if( ALL_PROCESS_EVENTS == _pid ){
        // NOTE: at this moment w/o filter. Two advantages:
        // - simple & stable database query
        // - fast filter criteria changing on client side
        query = BCON_NEW( nullptr );
    }
    else{
        query = BCON_NEW( mongo_fields::wal_process_events::PID.c_str(), BCON_INT32( _pid ));
    }

    mongoc_cursor_t * cursor = mongoc_collection_find(  m_tableWALProcessEvents,
                                                        MONGOC_QUERY_NONE,
                                                        0,
                                                        0,
                                                        1000000, // 10000 ~= inf
                                                        query,
                                                        nullptr,
                                                        nullptr );

    std::vector<common_types::SWALProcessEvent> out;
    const bson_t * doc;
    while( mongoc_cursor_next( cursor, & doc ) ){

        uint len;
        bson_iter_t iter;

        common_types::SWALProcessEvent oper;
        bson_iter_init_find( & iter, doc, mongo_fields::wal_process_events::PID.c_str() );
        oper.pid = bson_iter_int32( & iter );
        bson_iter_init_find( & iter, doc, mongo_fields::wal_process_events::PROGRAM_NAME.c_str() );
        oper.programName = bson_iter_utf8( & iter, & len );

        // array
        bson_iter_init_find( & iter, doc, mongo_fields::wal_process_events::PROGRAM_ARGS.c_str() );       
        uint childLen;
        bson_iter_t childIter;
        bson_iter_recurse( & iter, & childIter );
        while( bson_iter_next( & childIter ) ){
            const char * val = bson_iter_utf8( & childIter, & childLen );
            oper.programArgs.push_back( val );
        }
        // array

        out.push_back( oper );
    }

    mongoc_cursor_destroy( cursor );
    bson_destroy( query );

    return out;
}

std::vector<common_types::SWALProcessEvent> DatabaseManagerBase::getNonIntegrityProcessEvents(){

    // TODO: join this 2 queries
    // WTF 1: BCON_INT32(1) != "1"
    // WTF 2: "0" in BSON_APPEND_INT32( & arrayElement, "0", criteriaElement )

    // find single UKeys
    std::vector<common_types::TPid> singleUniqueKeys;
    {
        bson_t * pipeline = BCON_NEW( "pipeline", "[",
                                      "{", "$group", "{", "_id", "$pid", "count", "{", "$sum", BCON_INT32(1), "}", "}", "}",
                                      "{", "$match", "{", "count", "{", "$eq", BCON_INT32(1), "}", "}", "}",
                                      "]"
                                    );

        mongoc_cursor_t * cursor = mongoc_collection_aggregate( m_tableWALProcessEvents,
                                                                MONGOC_QUERY_NONE,
                                                                pipeline,
                                                                nullptr,
                                                                nullptr );
        const bson_t * doc;
        while( mongoc_cursor_next( cursor, & doc ) ){

            bson_iter_t iter;

            bson_iter_init_find( & iter, doc, "_id" );
            singleUniqueKeys.push_back( bson_iter_int32( & iter ) );
        }

        bson_destroy( pipeline );
        mongoc_cursor_destroy( cursor );
    }

    // get info about this keys
    std::vector<common_types::SWALProcessEvent> out;
    {
        bson_t criteria;
        bson_t arrayElement;
        bson_init( & criteria );
        bson_append_array_begin( & criteria, "$in", strlen("$in"), & arrayElement );
        for( const common_types::TPid & criteriaElement : singleUniqueKeys ){
            BSON_APPEND_INT32( & arrayElement, "0", criteriaElement );
        }
        bson_append_array_end( & criteria, & arrayElement );

        bson_t query;
        bson_init( & query );
        bson_append_document( & query, "pid", strlen("pid"), & criteria );

        mongoc_cursor_t * cursor = mongoc_collection_find(  m_tableWALProcessEvents,
                                                            MONGOC_QUERY_NONE,
                                                            0,
                                                            0,
                                                            1000000, // 10000 ~= inf
                                                            & query,
                                                            nullptr,
                                                            nullptr );

        const bson_t * doc;
        while( mongoc_cursor_next( cursor, & doc ) ){

            uint len;
            bson_iter_t iter;

            common_types::SWALProcessEvent oper;
            bson_iter_init_find( & iter, doc, mongo_fields::wal_process_events::PID.c_str() );
            oper.pid = bson_iter_int32( & iter );
            bson_iter_init_find( & iter, doc, mongo_fields::wal_process_events::PROGRAM_NAME.c_str() );
            oper.programName = bson_iter_utf8( & iter, & len );

            // array
            bson_iter_init_find( & iter, doc, mongo_fields::wal_process_events::PROGRAM_ARGS.c_str() );
            uint childLen;
            bson_iter_t childIter;
            bson_iter_recurse( & iter, & childIter );
            while( bson_iter_next( & childIter ) ){
                const char * val = bson_iter_utf8( & childIter, & childLen );
                oper.programArgs.push_back( val );
            }
            // array

            // TODO: ?
            bson_iter_init_find( & iter, doc, mongo_fields::wal_process_events::LAUNCHED.c_str() );
            const bool launched = bson_iter_bool( & iter );

            assert( launched );

            out.push_back( oper );
        }

        mongoc_cursor_destroy( cursor );
    }

    return out;
}

void DatabaseManagerBase::removeProcessEvent( common_types::TPid _pid ){

    bson_t * query = nullptr;
    if( ALL_PROCESS_EVENTS == _pid ){
        query = BCON_NEW( nullptr );
    }
    else{
        query = BCON_NEW( mongo_fields::wal_process_events::PID.c_str(), BCON_INT32( _pid ));
    }

    const bool result = mongoc_collection_remove( m_tableWALProcessEvents, MONGOC_REMOVE_NONE, query, nullptr, nullptr );

    if( ! result ){
        // TODO: do
    }

    bson_destroy( query );
}

bool DatabaseManagerBase::writeUserRegistration( const common_types::SWALUserRegistration & _registration ){

}

std::vector<common_types::SWALUserRegistration> DatabaseManagerBase::getUserRegistrations(){

}

void DatabaseManagerBase::removeUserRegistration( common_types::SWALUserRegistration::TRegisterId _id ){

}






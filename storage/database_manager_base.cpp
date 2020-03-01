
#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>

#include "system/logger.h"
#include "common/ms_common_vars.h"
#include "common/ms_common_utils.h"
#include "database_manager_astra.h"

using namespace std;
using namespace common_vars;

static constexpr const char * PRINT_HEADER = "DatabaseMgr:";
static const string ARGS_DELIMETER = "$";

bool DatabaseManager::m_systemInited = false;
int DatabaseManager::m_instanceCounter = 0;
const std::string DatabaseManager::ALL_CLIENT_OPERATIONS = "";
const common_types::TPid DatabaseManager::ALL_PROCESS_EVENTS = 0;

DatabaseManager::DatabaseManager()
    : m_mongoClient(nullptr)
    , m_database(nullptr)
{

}

DatabaseManager::~DatabaseManager()
{    
    mongoc_cleanup();

    for( mongoc_collection_t * collect : m_allCollections ){
        mongoc_collection_destroy( collect );
    }
    mongoc_database_destroy( m_database );
    mongoc_client_destroy( m_mongoClient );
}

void DatabaseManager::systemInit(){

    mongoc_init();    
    VS_LOG_INFO << PRINT_HEADER << " init success" << endl;
    m_systemInited = true;
}

// -------------------------------------------------------------------------------------
// service
// -------------------------------------------------------------------------------------
bool DatabaseManager::init( SInitSettings _settings ){

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

inline bool DatabaseManager::createIndex( const std::string & _tableName, const std::vector<std::string> & _fieldNames ){

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

// -------------------------------------------------------------------------------------
// WAL
// -------------------------------------------------------------------------------------
bool DatabaseManager::writeClientOperation( const common_types::SWALClientOperation & _operation ){

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

std::vector<common_types::SWALClientOperation> DatabaseManager::getClientOperations(){

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

std::vector<common_types::SWALClientOperation> DatabaseManager::getNonIntegrityClientOperations(){

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

void DatabaseManager::removeClientOperation( std::string _uniqueKey ){

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

bool DatabaseManager::writeProcessEvent( const common_types::SWALProcessEvent & _event, bool _launch ){

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

std::vector<common_types::SWALProcessEvent> DatabaseManager::getProcessEvents( common_types::TPid _pid ){

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

std::vector<common_types::SWALProcessEvent> DatabaseManager::getNonIntegrityProcessEvents(){

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

void DatabaseManager::removeProcessEvent( common_types::TPid _pid ){

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




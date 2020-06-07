
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
std::mutex DatabaseManagerBase::m_muStaticProtect;

// TODO: move away from database environment
const std::string DatabaseManagerBase::ALL_CLIENT_OPERATIONS = "";
const common_types::TPid DatabaseManagerBase::ALL_PROCESS_EVENTS = 0;
const std::string DatabaseManagerBase::ALL_REGISTRATION_IDS = "";

static string convertDataTypeToStr( const EPersistenceDataType _type ){
    switch( _type ){
    case EPersistenceDataType::TRAJECTORY : {
        return "traj";
    }
    case EPersistenceDataType::WEATHER : {
        return "weather";
    }
    default : {
        assert( false && "incorrect data type enum" );
    }
    }
}

static EPersistenceDataType convertDataTypeFromStr( const std::string & _str ){

    if( "traj" == _str ){
        return EPersistenceDataType::TRAJECTORY;
    }
    else if( "weather" == _str ){
        return EPersistenceDataType::WEATHER;
    }
    else{
        assert( false && "incorrect data type str" );
    }
}

DatabaseManagerBase::DatabaseManagerBase()
    : m_mongoClient(nullptr)
    , m_mongoDatabase(nullptr)
{

}

DatabaseManagerBase::~DatabaseManagerBase(){    

    mongoc_cleanup();

    for( mongoc_collection_t * collect : m_allTables ){
        mongoc_collection_destroy( collect );
    }
    mongoc_database_destroy( m_mongoDatabase );
    mongoc_client_destroy( m_mongoClient );
}

void DatabaseManagerBase::systemInit(){

    mongoc_init();    
    VS_LOG_INFO << PRINT_HEADER << " init success" << endl;
    m_systemInited = true;
}

DatabaseManagerBase * DatabaseManagerBase::getInstance(){

    m_muStaticProtect.lock();

    if( ! m_systemInited ){
        systemInit();
        m_systemInited = true;
    }
    m_instanceCounter++;

    m_muStaticProtect.unlock();
    return new DatabaseManagerBase();
}

void DatabaseManagerBase::destroyInstance( DatabaseManagerBase * & _inst ){

    m_muStaticProtect.lock();

    delete _inst;
    _inst = nullptr;
    m_instanceCounter--;

    m_muStaticProtect.unlock();
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

    m_mongoDatabase = mongoc_client_get_database( m_mongoClient, _settings.databaseName.c_str() );

    m_tableNamePrefix = _settings.databaseName + "_";

    m_tableWALClientOperations = mongoc_client_get_collection( m_mongoClient,
        _settings.databaseName.c_str(),
        (m_tableNamePrefix + mongo_fields::wal_client_operations::COLLECTION_NAME).c_str() );

    m_tableWALProcessEvents = mongoc_client_get_collection( m_mongoClient,
        _settings.databaseName.c_str(),
        (m_tableNamePrefix + mongo_fields::wal_process_events::COLLECTION_NAME).c_str() );

    m_tableWALUserRegistrations = mongoc_client_get_collection( m_mongoClient,
        _settings.databaseName.c_str(),
        (m_tableNamePrefix + mongo_fields::wal_user_registrations::COLLECTION_NAME).c_str() );

    m_tablePersistenceMetadata = mongoc_client_get_collection( m_mongoClient,
        _settings.databaseName.c_str(),
        (m_tableNamePrefix + mongo_fields::persistence_set_metadata::COLLECTION_NAME).c_str() );

    m_tablePersistenceDescription = mongoc_client_get_collection( m_mongoClient,
        _settings.databaseName.c_str(),
        (m_tableNamePrefix + mongo_fields::persistence_set_description::COLLECTION_NAME).c_str() );

    m_tablePersistenceFromVideo = mongoc_client_get_collection( m_mongoClient,
        _settings.databaseName.c_str(),
        (m_tableNamePrefix + mongo_fields::persistence_set_metadata_video::COLLECTION_NAME).c_str() );

    m_tablePersistenceFromRaw = mongoc_client_get_collection( m_mongoClient,
        _settings.databaseName.c_str(),
        (m_tableNamePrefix + mongo_fields::persistence_set_metadata_raw::COLLECTION_NAME).c_str() );

    m_tablePersistenceFromDSS = mongoc_client_get_collection( m_mongoClient,
        _settings.databaseName.c_str(),
        (m_tableNamePrefix + mongo_fields::persistence_set_metadata_dss::COLLECTION_NAME).c_str() );

    m_allTables.push_back( m_tableWALClientOperations );
    m_allTables.push_back( m_tableWALProcessEvents );
    m_allTables.push_back( m_tableWALUserRegistrations );
    m_allTables.push_back( m_tablePersistenceMetadata );
    m_allTables.push_back( m_tablePersistenceDescription );
    m_allTables.push_back( m_tablePersistenceFromVideo );
    m_allTables.push_back( m_tablePersistenceFromRaw );
    m_allTables.push_back( m_tablePersistenceFromDSS );

    initPayloadTableReferences();

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

    // create Q
    char * indexName = mongoc_collection_keys_to_index_string( & keys );
    bson_t * createIndex = BCON_NEW( "createIndexes",
                                     BCON_UTF8(_tableName.c_str()),
                                     "indexes", "[",
                                         "{", "key", BCON_DOCUMENT(& keys),
                                              "name", BCON_UTF8(indexName),
                                         "}",
                                     "]"
                                );

    // perform Q
    bson_t reply;
    bson_error_t error;
    const bool rt = mongoc_database_command_simple( m_mongoDatabase,
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

void DatabaseManagerBase::initPayloadTableReferences(){

    // make query
    bson_t * query = BCON_NEW( nullptr );
    mongoc_cursor_t * cursor = mongoc_collection_find( m_tablePersistenceMetadata,
            MONGOC_QUERY_NONE,
            0,
            0,
            0,
            query,
            nullptr,
            nullptr );

    // get results
    const bson_t * doc;
    while( mongoc_cursor_next( cursor, & doc ) ){
        uint len;
        bson_iter_t iter;

        bson_iter_init_find( & iter, doc, mongo_fields::persistence_set_metadata::SOURCE_TYPE.c_str() );
        const common_types::EPersistenceSourceType persType = common_utils::convertPersistenceTypeFromStr( bson_iter_utf8( & iter, & len ) );
        bson_iter_init_find( & iter, doc, mongo_fields::persistence_set_metadata::CTX_ID.c_str() );
        const TContextId ctxId = bson_iter_int32( & iter );
        bson_iter_init_find( & iter, doc, mongo_fields::persistence_set_metadata::MISSION_ID.c_str() );
        const TMissionId missionId = bson_iter_int32( & iter );
        bson_iter_init_find( & iter, doc, mongo_fields::persistence_set_metadata::UPDATE_STEP_MILLISEC.c_str() );
        const int64_t updateStepMillisec = bson_iter_int64( & iter );
        bson_iter_init_find( & iter, doc, mongo_fields::persistence_set_metadata::LAST_SESSION_ID.c_str() );
        const TSessionNum sessionNum = bson_iter_int32( & iter );
        bson_iter_init_find( & iter, doc, mongo_fields::persistence_set_metadata::PERSISTENCE_ID.c_str() );
        const TPersistenceSetId persId = bson_iter_int64( & iter );
        bson_iter_init_find( & iter, doc, mongo_fields::persistence_set_metadata::PAYLOAD_TABLE_NAME.c_str() );
        const std::string payloadTableName = bson_iter_utf8( & iter, & len );

        // reference to table
        createPayloadTableRef( persId, payloadTableName );
    }

    mongoc_cursor_destroy( cursor );
    bson_destroy( query );
}

inline void DatabaseManagerBase::createPayloadTableRef( common_types::TPersistenceSetId _persId, const std::string _tableName ){

    mongoc_collection_t * contextTable = mongoc_client_get_collection(
            m_mongoClient,
            m_settings.databaseName.c_str(),
            _tableName.c_str() );

    // TODO: each time when creating ref ?
    createIndex( _tableName, {mongo_fields::analytic::detected_object::SESSION,
            mongo_fields::analytic::detected_object::LOGIC_TIME}
            );

    m_tablesByPersistenceId.insert( {_persId, contextTable} );
    m_tableNameByPersistenceId.insert( {_persId, _tableName} );
}

inline mongoc_collection_t * DatabaseManagerBase::getPayloadTableRef( TPersistenceSetId _persId ){

    assert( _persId > 0 && m_tablesByPersistenceId.find(_persId) != m_tablesByPersistenceId.end() );
    return m_tablesByPersistenceId[ _persId ];
}

inline string DatabaseManagerBase::getTableName( common_types::TPersistenceSetId _persId ){

    assert( m_tableNameByPersistenceId.find(_persId) != m_tableNameByPersistenceId.end() );
    return m_tableNameByPersistenceId[ _persId ];
}

// -------------------------------------------------------------------------------------
// object payload
// -------------------------------------------------------------------------------------
bool DatabaseManagerBase::writeTrajectoryData( TPersistenceSetId _persId, const vector<SPersistenceTrajectory> & _data ){

    // get table
    mongoc_collection_t * contextTable = getPayloadTableRef( _persId );
    mongoc_bulk_operation_t * bulkedWrite = mongoc_collection_create_bulk_operation( contextTable, false, NULL );

    // build data into one set
    for( const SPersistenceTrajectory & traj : _data ){

        bson_t * doc = BCON_NEW( mongo_fields::analytic::detected_object::OBJRERP_ID.c_str(), BCON_INT64( traj.objId ),
                                 mongo_fields::analytic::detected_object::STATE.c_str(), BCON_INT32( (int32_t)(traj.state) ),
                                 mongo_fields::analytic::detected_object::ASTRO_TIME.c_str(), BCON_INT64( traj.astroTimeMillisec ),
                                 mongo_fields::analytic::detected_object::LOGIC_TIME.c_str(), BCON_INT64( traj.logicTime ),
                                 mongo_fields::analytic::detected_object::SESSION.c_str(), BCON_INT32( traj.sessionNum ),
                                 mongo_fields::analytic::detected_object::LAT.c_str(), BCON_DOUBLE( traj.latDeg ),
                                 mongo_fields::analytic::detected_object::LON.c_str(), BCON_DOUBLE( traj.lonDeg ),
                                 mongo_fields::analytic::detected_object::HEIGHT.c_str(), BCON_DOUBLE( traj.height ),
                                 mongo_fields::analytic::detected_object::YAW.c_str(), BCON_DOUBLE( traj.yawDeg )
                               );

        mongoc_bulk_operation_insert( bulkedWrite, doc );
        bson_destroy( doc );
    }

    // write
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

    mongoc_collection_t * contextTable = getPayloadTableRef( _filter.persistenceSetId );
    assert( contextTable );

    bson_t * projection = nullptr;
    bson_t * query = nullptr;

    // only one step
    if( (_filter.minLogicStep == _filter.maxLogicStep) && _filter.minLogicStep >= 0 ){
        query = BCON_NEW( "$and", "[", "{", mongo_fields::analytic::detected_object::SESSION.c_str(), BCON_INT32(_filter.sessionNum), "}",
                                       "{", mongo_fields::analytic::detected_object::LOGIC_TIME.c_str(), "{", "$eq", BCON_INT64(_filter.minLogicStep), "}", "}",
                                  "]"
                        );
    }
    // steps range
    else if( _filter.minLogicStep >= 0 && _filter.maxLogicStep >= 0 ){
        query = BCON_NEW( "$and", "[", "{", mongo_fields::analytic::detected_object::SESSION.c_str(), BCON_INT32(_filter.sessionNum), "}",
                                       "{", mongo_fields::analytic::detected_object::LOGIC_TIME.c_str(), "{", "$gte", BCON_INT64(_filter.minLogicStep), "}", "}",
                                       "{", mongo_fields::analytic::detected_object::LOGIC_TIME.c_str(), "{", "$lte", BCON_INT64(_filter.maxLogicStep), "}", "}",
                                        "]"
                        );
    }
    // whole area
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
        detectedObject.astroTimeMillisec = bson_iter_int64( & iter );
        bson_iter_init_find( & iter, doc, mongo_fields::analytic::detected_object::LOGIC_TIME.c_str() );
        detectedObject.logicTime = bson_iter_int64( & iter );
        bson_iter_init_find( & iter, doc, mongo_fields::analytic::detected_object::SESSION.c_str() );
        detectedObject.sessionNum = bson_iter_int32( & iter );
        bson_iter_init_find( & iter, doc, mongo_fields::analytic::detected_object::STATE.c_str() );
        detectedObject.state = (SPersistenceObj::EState)bson_iter_int32( & iter );

        if( detectedObject.state == SPersistenceObj::EState::ACTIVE ){
            bson_iter_init_find( & iter, doc, mongo_fields::analytic::detected_object::LAT.c_str() );
            detectedObject.latDeg = bson_iter_double( & iter );
            bson_iter_init_find( & iter, doc, mongo_fields::analytic::detected_object::LON.c_str() );
            detectedObject.lonDeg = bson_iter_double( & iter );
            bson_iter_init_find( & iter, doc, mongo_fields::analytic::detected_object::HEIGHT.c_str() );
            detectedObject.height = bson_iter_double( & iter );
            bson_iter_init_find( & iter, doc, mongo_fields::analytic::detected_object::YAW.c_str() );
            detectedObject.yawDeg = bson_iter_double( & iter );
        }

        out.push_back( detectedObject );
    }

    mongoc_cursor_destroy( cursor );
    bson_destroy( query );

    return out;
}

void DatabaseManagerBase::deleteDataRange( const SPersistenceSetFilter & _filter ){

    // TODO: remove by logic step range

    mongoc_collection_t * contextTable = getPayloadTableRef( _filter.persistenceSetId );
    assert( contextTable );

    bson_t * query = BCON_NEW( nullptr );

    bson_error_t error;
    const bool result = mongoc_collection_remove( contextTable, MONGOC_REMOVE_NONE, query, nullptr, & error );
    if( ! result ){
        VS_LOG_ERROR << PRINT_HEADER << " total delete failed, reason: " << error.message << endl;
    }

    bson_destroy( query );
}

void DatabaseManagerBase::deleteTotalData( const common_types::TContextId _ctxId ){

    // get persistenceId of this contextId
    const vector<SPersistenceMetadata> metadatas = getPersistenceSetMetadata( _ctxId );
    for( const SPersistenceMetadata & meta : metadatas ){
        for( const SPersistenceMetadataDSS & metaDSS : meta.persistenceFromDSS ){
            deleteDataRange( SPersistenceSetFilter(metaDSS.persistenceSetId) );
        }

        for( const SPersistenceMetadataRaw & metaRaw : meta.persistenceFromRaw ){
            deleteDataRange( SPersistenceSetFilter(metaRaw.persistenceSetId) );
        }

        for( const SPersistenceMetadataVideo & metaVideo : meta.persistenceFromVideo ){
            deleteDataRange( SPersistenceSetFilter(metaVideo.persistenceSetId) );
        }
    }
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

// -------------------------------------------------------------------------------------
// persistence metadata
// -------------------------------------------------------------------------------------

// persistence write
TPersistenceSetId DatabaseManagerBase::writePersistenceSetMetadata( const common_types::SPersistenceMetadataVideo & _videoMetadata ){

    const string payloadTableName = m_tableNamePrefix
            + string("video_")
            + convertDataTypeToStr(_videoMetadata.dataType) + string("_")
            + string("sensor")
            + std::to_string(_videoMetadata.recordedFromSensorId);

    // update existing PersistenceId ( if it valid of course )
    if( _videoMetadata.persistenceSetId != SPersistenceMetadataDescr::INVALID_PERSISTENCE_ID ){

        if( isPersistenceMetadataValid(_videoMetadata.persistenceSetId, _videoMetadata) ){
            writePersistenceMetadataGlobal( _videoMetadata.persistenceSetId, payloadTableName, _videoMetadata );
            writePersistenceFromVideo( _videoMetadata );

            return _videoMetadata.persistenceSetId;
        }
        else{
            return SPersistenceMetadataDescr::INVALID_PERSISTENCE_ID;
        }
    }
    // create new persistence record
    else{
        const TPersistenceSetId persId = createNewPersistenceId();

        writePersistenceMetadataGlobal( persId, payloadTableName, _videoMetadata );
        writePersistenceFromVideo( _videoMetadata );
        createPayloadTableRef( persId, payloadTableName );

        return persId;
    }
}

TPersistenceSetId DatabaseManagerBase::writePersistenceSetMetadata( const common_types::SPersistenceMetadataDSS & _dssMetadata ){

    const string payloadTableName = m_tableNamePrefix
            + string("dss_")
            + convertDataTypeToStr(_dssMetadata.dataType) + string("_")
            + string("ctx")
            + std::to_string(_dssMetadata.contextId)
            + string("_mission")
            + std::to_string(_dssMetadata.missionId);
            + (_dssMetadata.realData ? "_real" : "_simula");

    // update existing PersistenceId ( if it valid of course )
    if( _dssMetadata.persistenceSetId != SPersistenceMetadataDescr::INVALID_PERSISTENCE_ID ){

        if( isPersistenceMetadataValid(_dssMetadata.persistenceSetId, _dssMetadata) ){
            writePersistenceMetadataGlobal( _dssMetadata.persistenceSetId, payloadTableName, _dssMetadata );
            writePersistenceFromDSS( _dssMetadata );

            return _dssMetadata.persistenceSetId;
        }
        else{
            return SPersistenceMetadataDescr::INVALID_PERSISTENCE_ID;
        }
    }
    // create new persistence record
    else{
        const TPersistenceSetId persId = createNewPersistenceId();

        writePersistenceMetadataGlobal( persId, payloadTableName, _dssMetadata );
        writePersistenceFromDSS( _dssMetadata );
        createPayloadTableRef( persId, payloadTableName );

        return persId;
    }
}

TPersistenceSetId DatabaseManagerBase::writePersistenceSetMetadata( const common_types::SPersistenceMetadataRaw & _rawMetadata ){

    const string payloadTableName = m_tableNamePrefix
            + string("raw_")
            + convertDataTypeToStr(_rawMetadata.dataType) + string("_")
            + string("ctx")
            + std::to_string(_rawMetadata.contextId)
            + string("_mission")
            + std::to_string(_rawMetadata.missionId);

    // update existing PersistenceId ( if it valid of course )
    if( _rawMetadata.persistenceSetId != common_vars::INVALID_PERS_ID ){

        if( isPersistenceMetadataValid(_rawMetadata.persistenceSetId, _rawMetadata) ){
            writePersistenceMetadataGlobal( _rawMetadata.persistenceSetId, payloadTableName, _rawMetadata );
            writePersistenceFromRaw( _rawMetadata );

            return _rawMetadata.persistenceSetId;
        }
        else{
            return SPersistenceMetadataDescr::INVALID_PERSISTENCE_ID;
        }
    }
    // create new persistence record
    else{
        const TPersistenceSetId persId = createNewPersistenceId();

        writePersistenceMetadataGlobal( persId, payloadTableName, _rawMetadata );
        writePersistenceFromRaw( _rawMetadata );
        createPayloadTableRef( persId, payloadTableName );

        return persId;
    }
}

void DatabaseManagerBase::writePersistenceMetadataGlobal( const common_types::TPersistenceSetId _persId,
                                                          const std::string _payloadTableName,
                                                          const common_types::SPersistenceMetadataDescr & _meta ){

    // TODO: temporary input validation
    assert( _meta.contextId > 0
            && _meta.lastRecordedSession > 0
            && _meta.timeStepIntervalMillisec > 0
            && _persId > 0
            && _meta.sourceType != EPersistenceSourceType::UNDEFINED
            && "global metadata validation" );

    bson_t * query = BCON_NEW( mongo_fields::persistence_set_metadata::PERSISTENCE_ID.c_str(), BCON_INT64( _persId ) );
    bson_t * update = BCON_NEW( "$set", "{",
            mongo_fields::persistence_set_metadata::PERSISTENCE_ID.c_str(), BCON_INT64( _persId ),
            mongo_fields::persistence_set_metadata::CTX_ID.c_str(), BCON_INT32( _meta.contextId ),
            mongo_fields::persistence_set_metadata::MISSION_ID.c_str(), BCON_INT32( _meta.missionId ),
            mongo_fields::persistence_set_metadata::LAST_SESSION_ID.c_str(), BCON_INT32( _meta.lastRecordedSession ),
            mongo_fields::persistence_set_metadata::UPDATE_STEP_MILLISEC.c_str(), BCON_INT64( _meta.timeStepIntervalMillisec ),
            mongo_fields::persistence_set_metadata::SOURCE_TYPE.c_str(), BCON_UTF8( common_utils::convertPersistenceTypeToStr(_meta.sourceType).c_str() ),
            mongo_fields::persistence_set_metadata::DATA_TYPE.c_str(), BCON_UTF8( convertDataTypeToStr(_meta.dataType).c_str() ),
            mongo_fields::persistence_set_metadata::PAYLOAD_TABLE_NAME.c_str(), BCON_UTF8( _payloadTableName.c_str() ),
                            "}" );

    bson_error_t error;
    const bool rt = mongoc_collection_update( m_tablePersistenceMetadata,
                                  MONGOC_UPDATE_UPSERT,
                                  query,
                                  update,
                                  NULL,
                                  & error );

    if( ! rt ){
        VS_LOG_ERROR << PRINT_HEADER << " global metadata write failed, reason: " << error.message << endl;
        bson_destroy( query );
        bson_destroy( update );
        return;
    }

    bson_destroy( query );
    bson_destroy( update );
}

void DatabaseManagerBase::writePersistenceFromVideo( const common_types::SPersistenceMetadataVideo & _videoMetadata ){

    bson_t * query = BCON_NEW( mongo_fields::persistence_set_metadata::PERSISTENCE_ID.c_str(), BCON_INT64( _videoMetadata.persistenceSetId ) );
    bson_t * update = BCON_NEW( "$set", "{",
            mongo_fields::persistence_set_metadata::PERSISTENCE_ID.c_str(), BCON_INT64( _videoMetadata.persistenceSetId ),
            mongo_fields::persistence_set_metadata_video::SENSOR_ID.c_str(), BCON_INT64( _videoMetadata.recordedFromSensorId ),
            "}" );

    bson_error_t error;
    const bool rt = mongoc_collection_update( m_tablePersistenceFromVideo,
                                  MONGOC_UPDATE_UPSERT,
                                  query,
                                  update,
                                  NULL,
                                  & error );

    if( ! rt ){
        VS_LOG_ERROR << PRINT_HEADER <<" video metadata write failed, reason: " << error.message << endl;
        bson_destroy( query );
        bson_destroy( update );
        return;
    }

    bson_destroy( query );
    bson_destroy( update );
}

void DatabaseManagerBase::writePersistenceFromDSS( const common_types::SPersistenceMetadataDSS & _dssMetadata ){

    bson_t * query = BCON_NEW( mongo_fields::persistence_set_metadata::PERSISTENCE_ID.c_str(), BCON_INT64( _dssMetadata.persistenceSetId ) );
    bson_t * update = BCON_NEW( "$set", "{",
            mongo_fields::persistence_set_metadata::PERSISTENCE_ID.c_str(), BCON_INT64( _dssMetadata.persistenceSetId ),
            mongo_fields::persistence_set_metadata_dss::REAL.c_str(), BCON_BOOL( _dssMetadata.realData ),
            "}" );

    bson_error_t error;
    const bool rt = mongoc_collection_update( m_tablePersistenceFromDSS,
                                  MONGOC_UPDATE_UPSERT,
                                  query,
                                  update,
                                  NULL,
                                  & error );

    if( ! rt ){
        VS_LOG_ERROR << PRINT_HEADER <<" dss metadata write failed, reason: " << error.message << endl;
        bson_destroy( query );
        bson_destroy( update );
        return;
    }

    bson_destroy( query );
    bson_destroy( update );
}

void DatabaseManagerBase::writePersistenceFromRaw( const common_types::SPersistenceMetadataRaw & _type ){

    // NOTE: no raw metadata needed yet
    return;
}

// persistence read
std::vector<SPersistenceMetadata> DatabaseManagerBase::getPersistenceSetMetadata( common_types::TContextId _ctxId ){

//    bson_t * query = nullptr;
//    bson_t * sortOrder = nullptr;
//    if( _ctxId == common_vars::ALL_CONTEXT_ID ){
//        query = BCON_NEW( nullptr );
//        sortOrder = BCON_NEW( "sort", "{", mongo_fields::persistence_set_metadata::CTX_ID.c_str(), BCON_INT32 (-1), "}" );
//    }
//    else{
//        query = BCON_NEW( mongo_fields::persistence_set_metadata::CTX_ID.c_str(), BCON_INT32( _ctxId ));
//        sortOrder = BCON_NEW( nullptr );
//    }

//    mongoc_cursor_t * cursor = mongoc_collection_find_with_opts( m_tablePersistenceDescr,
//                                                                 query,
//                                                                 sortOrder,
//                                                                 nullptr );

    bson_t * query = nullptr;
    if( _ctxId == common_vars::ALL_CONTEXT_ID ){
        query = BCON_NEW( "$query", "{", "}",
                          "$orderby", "{", mongo_fields::persistence_set_metadata::CTX_ID.c_str(), BCON_INT32 (-1), "}" );
    }
    else{
        query = BCON_NEW( "$query", "{", mongo_fields::persistence_set_metadata::CTX_ID.c_str(), BCON_INT32( _ctxId ), "}" );
    }

    mongoc_cursor_t * cursor = mongoc_collection_find( m_tablePersistenceMetadata,
            MONGOC_QUERY_NONE,
            0,
            0,
            0,
            query,
            nullptr,
            nullptr );

    std::vector<common_types::SPersistenceMetadata> out;
    TContextId currentCtxId = 0;
    int currentStoreIdx = -1;

    const bson_t * doc;
    while( mongoc_cursor_next( cursor, & doc ) ){

        uint len;
        bson_iter_t iter;

        bson_iter_init_find( & iter, doc, mongo_fields::persistence_set_metadata::SOURCE_TYPE.c_str() );
        const common_types::EPersistenceSourceType persType = common_utils::convertPersistenceTypeFromStr( bson_iter_utf8( & iter, & len ) );
        bson_iter_init_find( & iter, doc, mongo_fields::persistence_set_metadata::DATA_TYPE.c_str() );
        const EPersistenceDataType dataType = convertDataTypeFromStr( bson_iter_utf8( & iter, & len ) );
        bson_iter_init_find( & iter, doc, mongo_fields::persistence_set_metadata::CTX_ID.c_str() );
        const TContextId ctxId = bson_iter_int32( & iter );
        bson_iter_init_find( & iter, doc, mongo_fields::persistence_set_metadata::MISSION_ID.c_str() );
        const TMissionId missionId = bson_iter_int32( & iter );
        bson_iter_init_find( & iter, doc, mongo_fields::persistence_set_metadata::UPDATE_STEP_MILLISEC.c_str() );
        const int64_t updateStepMillisec = bson_iter_int64( & iter );
        bson_iter_init_find( & iter, doc, mongo_fields::persistence_set_metadata::LAST_SESSION_ID.c_str() );
        const TSessionNum sessionNum = bson_iter_int32( & iter );
        bson_iter_init_find( & iter, doc, mongo_fields::persistence_set_metadata::PERSISTENCE_ID.c_str() );
        const TPersistenceSetId persId = bson_iter_int64( & iter );
        bson_iter_init_find( & iter, doc, mongo_fields::persistence_set_metadata::PAYLOAD_TABLE_NAME.c_str() );
        const std::string payloadTableName = bson_iter_utf8( & iter, & len );

        //
        if( ctxId != currentCtxId ){
            currentCtxId = ctxId;
            out.resize( out.size() + 1 );
            currentStoreIdx++;
        }

        //
        switch( persType ){
        case common_types::EPersistenceSourceType::VIDEO_SERVER : {


            break;
        }
        case common_types::EPersistenceSourceType::AUTONOMOUS_RECORDER : {
            common_types::SPersistenceMetadata & currentCtxMetadata = out[ currentStoreIdx ];
            currentCtxMetadata.persistenceFromRaw.resize( currentCtxMetadata.persistenceFromRaw.size() + 1 );
            common_types::SPersistenceMetadataRaw & currentRawMetadata = currentCtxMetadata.persistenceFromRaw.back();

            // video specific parameters
            const bool rt = getPersistenceFromRaw( persId, currentRawMetadata );

            // global parameters
            currentRawMetadata.persistenceSetId = persId;
            currentRawMetadata.contextId = ctxId;
            currentRawMetadata.missionId = missionId;
            currentRawMetadata.timeStepIntervalMillisec = updateStepMillisec;
            currentRawMetadata.lastRecordedSession = sessionNum;
            currentRawMetadata.sourceType = persType;
            currentRawMetadata.dataType = dataType;
            // ...

            break;
        }
        case common_types::EPersistenceSourceType::DSS : {

            break;
        }
        default : {

        }
        }
    }

    mongoc_cursor_destroy( cursor );
    bson_destroy( query );

    return out;
}

common_types::SPersistenceMetadata DatabaseManagerBase::getPersistenceSetMetadata( common_types::TPersistenceSetId _persId ){

    // make query
//    bson_t * query = BCON_NEW( mongo_fields::persistence_set_metadata::PERSISTENCE_ID.c_str(), BCON_INT32( _persId ));

//    mongoc_cursor_t * cursor = mongoc_collection_find_with_opts( m_tablePersistenceDescr,
//                                                                 query,
//                                                                 nullptr,
//                                                                 nullptr );
    bson_t * query = BCON_NEW( "$query", "{", mongo_fields::persistence_set_metadata::PERSISTENCE_ID.c_str(), BCON_INT32( _persId ), "}" );

    mongoc_cursor_t * cursor = mongoc_collection_find( m_tablePersistenceMetadata,
            MONGOC_QUERY_NONE,
            0,
            0,
            0,
            query,
            nullptr,
            nullptr );

    // check
    if( ! mongoc_cursor_more( cursor ) ){
        return common_types::SPersistenceMetadata();
    }

    // read
    common_types::SPersistenceMetadata out;

    const bson_t * doc;
    while( mongoc_cursor_next( cursor, & doc ) ){

        uint len;
        bson_iter_t iter;

        bson_iter_init_find( & iter, doc, mongo_fields::persistence_set_metadata::SOURCE_TYPE.c_str() );
        const common_types::EPersistenceSourceType persType = common_utils::convertPersistenceTypeFromStr( bson_iter_utf8( & iter, & len ) );
        bson_iter_init_find( & iter, doc, mongo_fields::persistence_set_metadata::CTX_ID.c_str() );
        const TContextId ctxId = bson_iter_int32( & iter );
        bson_iter_init_find( & iter, doc, mongo_fields::persistence_set_metadata::MISSION_ID.c_str() );
        const TMissionId missionId = bson_iter_int32( & iter );
        bson_iter_init_find( & iter, doc, mongo_fields::persistence_set_metadata::UPDATE_STEP_MILLISEC.c_str() );
        const int64_t updateStepMillisec = bson_iter_int64( & iter );
        bson_iter_init_find( & iter, doc, mongo_fields::persistence_set_metadata::LAST_SESSION_ID.c_str() );
        const TSessionNum sessionNum = bson_iter_int64( & iter );
        bson_iter_init_find( & iter, doc, mongo_fields::persistence_set_metadata::PERSISTENCE_ID.c_str() );
        const TPersistenceSetId persId = bson_iter_int64( & iter );

        // only ONE of this types will be appear
        switch( persType ){
        case common_types::EPersistenceSourceType::VIDEO_SERVER : {
            // TODO: do
            break;
        }
        case common_types::EPersistenceSourceType::AUTONOMOUS_RECORDER : {
            out.persistenceFromRaw.resize( out.persistenceFromRaw.size() + 1 );
            common_types::SPersistenceMetadataRaw & currentRawMetadata = out.persistenceFromRaw.back();

            // raw specific parameters
            const bool rt = getPersistenceFromRaw( persId, currentRawMetadata );

            // global parameters
            currentRawMetadata.persistenceSetId = persId;
            currentRawMetadata.contextId = ctxId;
            currentRawMetadata.missionId = missionId;
            currentRawMetadata.timeStepIntervalMillisec = updateStepMillisec;
            currentRawMetadata.lastRecordedSession = sessionNum;
            currentRawMetadata.sourceType = persType;
            // ...

            break;
        }
        case common_types::EPersistenceSourceType::DSS : {
            // TODO: do
            break;
        }
        default : {

        }
        }
    }

    mongoc_cursor_destroy( cursor );
    bson_destroy( query );

    return out;
}

bool DatabaseManagerBase::getPersistenceFromVideo( common_types::TPersistenceSetId _persId,
                                                   SPersistenceMetadataVideo & _meta ){

    // query
    bson_t * query = BCON_NEW( mongo_fields::persistence_set_metadata::PERSISTENCE_ID, BCON_INT32( _persId ) );
    mongoc_cursor_t * cursor = mongoc_collection_find(  m_tablePersistenceFromVideo,
                                                        MONGOC_QUERY_NONE,
                                                        0,
                                                        0,
                                                        1000000, // 10000 ~= inf
                                                        query,
                                                        nullptr,
                                                        nullptr );

    // check existence
    if( ! mongoc_cursor_more(cursor) ){
        VS_LOG_ERROR << PRINT_HEADER << " such persistence id (video) not found: " << _persId << endl;
        return false;
    }

    // get data
    const bson_t * doc;
    while( mongoc_cursor_next( cursor, & doc ) ){

        uint len;
        bson_iter_t iter;

        bson_iter_init_find( & iter, doc, mongo_fields::persistence_set_metadata_video::SENSOR_ID.c_str() );
        _meta.recordedFromSensorId = bson_iter_int32( & iter );
    }

    mongoc_cursor_destroy( cursor );
    bson_destroy( query );

    return true;
}

bool DatabaseManagerBase::getPersistenceFromDSS( common_types::TPersistenceSetId _persId,
                                                 common_types::SPersistenceMetadataDSS & _meta ){



}

bool DatabaseManagerBase::getPersistenceFromRaw( common_types::TPersistenceSetId _persId,
                                                 common_types::SPersistenceMetadataRaw & _meta ){

    // TODO: no raw metadata needed yet
    return true;
}


// persistence delete
void DatabaseManagerBase::deletePersistenceSetMetadata( common_types::TPersistenceSetId _id ){

    // delete from 'spec table' and ...
    const common_types::SPersistenceMetadata metadata = getPersistenceSetMetadata( _id );
    if( ! metadata.persistenceFromDSS.empty() ){
        deletePersistenceFromDSS( _id );
    }
    else if( ! metadata.persistenceFromRaw.empty() ){
        deletePersistenceFromRaw( _id );
    }
    else if( ! metadata.persistenceFromVideo.empty() ){
        deletePersistenceFromVideo( _id );
    }
    else{
        VS_LOG_CRITICAL << PRINT_HEADER << " persistence id not found for deleting: " << _id << endl;
        assert( false && "persistence id not found for deleting" );
    }

    // ... and from global 'metadata table'
    bson_t * query = BCON_NEW( mongo_fields::persistence_set_metadata::PERSISTENCE_ID.c_str(), BCON_INT64(_id) );

    bson_error_t error;
    const bool result = mongoc_collection_remove( m_tablePersistenceMetadata, MONGOC_REMOVE_NONE, query, nullptr, & error );
    if( ! result ){
        VS_LOG_ERROR << PRINT_HEADER << " persistence id deleting failed, reason: " << error.message << endl;
    }

    bson_destroy( query );
}

void DatabaseManagerBase::deletePersistenceSetMetadata( common_types::TContextId _ctxId ){

    // delete all persId for this ctxId in 'spec tables'
    const vector<SPersistenceMetadata> metadatas = getPersistenceSetMetadata( _ctxId );
    for( const SPersistenceMetadata & meta : metadatas ){
        for( const SPersistenceMetadataDSS & metaDSS : meta.persistenceFromDSS ){
            deletePersistenceFromDSS( metaDSS.persistenceSetId );
        }

        for( const SPersistenceMetadataRaw & metaRaw : meta.persistenceFromRaw ){
            deletePersistenceFromRaw( metaRaw.persistenceSetId );
        }

        for( const SPersistenceMetadataVideo & metaVideo : meta.persistenceFromVideo ){
            deletePersistenceFromVideo( metaVideo.persistenceSetId );
        }
    }

    // ... and from global 'metadata table'
    bson_t * query = BCON_NEW( mongo_fields::persistence_set_metadata::CTX_ID.c_str(), BCON_INT32(_ctxId) );

    bson_error_t error;
    const bool result = mongoc_collection_remove( m_tablePersistenceMetadata, MONGOC_REMOVE_NONE, query, nullptr, & error );
    if( ! result ){
        VS_LOG_ERROR << PRINT_HEADER << " context id deleting failed, reason: " << error.message << endl;
    }

    bson_destroy( query );
}

void DatabaseManagerBase::deletePersistenceFromRaw( common_types::TPersistenceSetId _persId ){

    // NOTE: there is no specific data for raw type yet
}

void DatabaseManagerBase::deletePersistenceFromDSS( common_types::TPersistenceSetId _persId ){

    // TODO: delete real / simulate record
}

void DatabaseManagerBase::deletePersistenceFromVideo( common_types::TPersistenceSetId _persId ){

    // TODO: delete sensor record
}

// persistence utils
bool DatabaseManagerBase::isPersistenceMetadataValid( common_types::TPersistenceSetId _persId, const common_types::SPersistenceMetadataDescr & _meta ){

    // for instance: forbid CtxId changing

    const SPersistenceMetadata metadata = getPersistenceSetMetadata( _persId );



    VS_LOG_ERROR << PRINT_HEADER << " such persistence id not found in global metadata" << endl;



    return true;
}

common_types::TPersistenceSetId DatabaseManagerBase::createNewPersistenceId(){

    const vector<SPersistenceMetadata> metadatas = getPersistenceSetMetadata();
    if( ! metadatas.empty() ){
        TPersistenceSetId newId = SPersistenceMetadataDescr::INVALID_PERSISTENCE_ID;

        // WTF?

        return newId;
    }
    else{
        const TPersistenceSetId newId = 1;
        return newId;
    }
}

bool DatabaseManagerBase::insertSessionDescription( const TPersistenceSetId _persId, const SEventsSessionInfo & _descr ){

    // ----------------------------------------------------------------------------------------------
    // make shure that session num is not exist
    // ----------------------------------------------------------------------------------------------
    if( isSessionExistInDescription(_descr.number) ){
        VS_LOG_ERROR << PRINT_HEADER << " insert description failed, such session num [" << _descr.number << "] ALREADY exist" << endl;
        return false;
    }

    // ----------------------------------------------------------------------------------------------
    // add session
    // ----------------------------------------------------------------------------------------------
    bson_t * doc = BCON_NEW( mongo_fields::persistence_set_description::PERSISTENCE_ID.c_str(), BCON_INT64( _persId),
                             mongo_fields::persistence_set_description::SESSION_NUM.c_str(), BCON_INT32( _descr.number ),
                             mongo_fields::persistence_set_description::LOGIC_TIME_MIN.c_str(), BCON_INT64( _descr.minLogicStep ),
                             mongo_fields::persistence_set_description::LOGIC_TIME_MAX.c_str(), BCON_INT64( _descr.maxLogicStep ),
                             mongo_fields::persistence_set_description::ASTRO_TIME_MIN.c_str(), BCON_INT64( _descr.minTimestampMillisec ),
                             mongo_fields::persistence_set_description::ASTRO_TIME_MAX.c_str(), BCON_INT64( _descr.maxTimestampMillisec ),
                             mongo_fields::persistence_set_description::EMPTY_STEPS_BEGIN.c_str(), BCON_INT32( 0 ),
                             mongo_fields::persistence_set_description::EMPTY_STEPS_END.c_str(), BCON_INT32( 0 )
                           );

    bson_error_t error;
    const bool rt = mongoc_collection_insert( m_tablePersistenceDescription,
                                              MONGOC_INSERT_NONE,
                                              doc,
                                              NULL,
                                              & error );

    if( 0 == rt ){
        VS_LOG_ERROR << PRINT_HEADER << " session description insert failed, reason: " << error.message << endl;
        bson_destroy( doc );
        return false;
    }

    bson_destroy( doc );
    return true;
}

bool DatabaseManagerBase::updateSessionDescription( const TPersistenceSetId _persId, const SEventsSessionInfo & _descr ){

    // ----------------------------------------------------------------------------------------------
    // check that session num exist
    // ----------------------------------------------------------------------------------------------
    if( ! isSessionExistInDescription(_descr.number) ){
        VS_LOG_ERROR << PRINT_HEADER << " update description failed, such session num [" << _descr.number << "] is NOT exist" << endl;
        return false;
    }

    // ----------------------------------------------------------------------------------------------
    // udpate steps
    // ----------------------------------------------------------------------------------------------
    bson_t * query = BCON_NEW( mongo_fields::persistence_set_description::SESSION_NUM.c_str(), BCON_INT32( _descr.number ) );
    bson_t * update = BCON_NEW( "$set", "{",
                                mongo_fields::persistence_set_description::PERSISTENCE_ID.c_str(), BCON_INT64( _persId ),
                                mongo_fields::persistence_set_description::SESSION_NUM.c_str(), BCON_INT32( _descr.number ),
                                mongo_fields::persistence_set_description::LOGIC_TIME_MIN.c_str(), BCON_INT64( _descr.minLogicStep ),
                                mongo_fields::persistence_set_description::LOGIC_TIME_MAX.c_str(), BCON_INT64( _descr.maxLogicStep ),
                                mongo_fields::persistence_set_description::ASTRO_TIME_MIN.c_str(), BCON_INT64( _descr.minTimestampMillisec ),
                                mongo_fields::persistence_set_description::ASTRO_TIME_MAX.c_str(), BCON_INT64( _descr.maxTimestampMillisec ),
                                mongo_fields::persistence_set_description::EMPTY_STEPS_BEGIN.c_str(), BCON_INT32( 0 ),
                                mongo_fields::persistence_set_description::EMPTY_STEPS_END.c_str(), BCON_INT32( 0 ),
                              "}" );

    bson_error_t error;
    const bool rt = mongoc_collection_update( m_tablePersistenceDescription,
                                  MONGOC_UPDATE_NONE,
                                  query,
                                  update,
                                  NULL,
                                  & error );

    if( ! rt ){
        VS_LOG_ERROR << PRINT_HEADER << " update session description failed, reason: " << error.message << endl;
        bson_destroy( query );
        bson_destroy( update );
        return false;
    }

    bson_destroy( query );
    bson_destroy( update );
    return true;
}

vector<SEventsSessionInfo> DatabaseManagerBase::selectSessionDescriptions( const TPersistenceSetId _persId ){

    bson_t * query = BCON_NEW( mongo_fields::persistence_set_description::PERSISTENCE_ID.c_str(), BCON_INT64( _persId ));

    mongoc_cursor_t * cursor = mongoc_collection_find(  m_tablePersistenceDescription,
                                                        MONGOC_QUERY_NONE,
                                                        0,
                                                        0,
                                                        1000000, // 10000 ~= inf
                                                        query,
                                                        nullptr,
                                                        nullptr );

    std::vector<SEventsSessionInfo> out;
    const bson_t * doc;
    while( mongoc_cursor_next( cursor, & doc ) ){
        bson_iter_t iter;

        SEventsSessionInfo info;
        bson_iter_init_find( & iter, doc, mongo_fields::persistence_set_description::SESSION_NUM.c_str() );
        info.number = bson_iter_int32( & iter );
        bson_iter_init_find( & iter, doc, mongo_fields::persistence_set_description::LOGIC_TIME_MIN.c_str() );
        info.minLogicStep = bson_iter_int64( & iter );
        bson_iter_init_find( & iter, doc, mongo_fields::persistence_set_description::LOGIC_TIME_MAX.c_str() );
        info.maxLogicStep = bson_iter_int64( & iter );
        bson_iter_init_find( & iter, doc, mongo_fields::persistence_set_description::ASTRO_TIME_MIN.c_str() );
        info.minTimestampMillisec = bson_iter_int64( & iter );
        bson_iter_init_find( & iter, doc, mongo_fields::persistence_set_description::ASTRO_TIME_MAX.c_str() );
        info.maxTimestampMillisec = bson_iter_int64( & iter );
        bson_iter_init_find( & iter, doc, mongo_fields::persistence_set_description::EMPTY_STEPS_BEGIN.c_str() );
        info.emptyStepsBegin = bson_iter_int32( & iter );
        bson_iter_init_find( & iter, doc, mongo_fields::persistence_set_description::EMPTY_STEPS_END.c_str() );
        info.emptyStepsEnd = bson_iter_int32( & iter );

        out.push_back( info );
    }

    mongoc_cursor_destroy( cursor );
    bson_destroy( query );

    return out;
}

common_types::SEventsSessionInfo DatabaseManagerBase::scanPayloadHeadForSessions( const common_types::TPersistenceSetId _persId ){

    const string tableName = getTableName(_persId);
    bson_t * cmd = BCON_NEW(    "distinct", BCON_UTF8( tableName.c_str() ),
                                "key", BCON_UTF8( mongo_fields::analytic::detected_object::SESSION.c_str() )
                            );

    bson_t reply;
    bson_error_t error;
    const bool rt = mongoc_database_command_simple( m_mongoDatabase,
                                                    cmd,
                                                    NULL,
                                                    & reply,
                                                    & error );

    if( 0 == rt ){
        VS_LOG_ERROR << PRINT_HEADER << " scanPayloadHeadForSessions failed, reason: " << error.message << endl;
        bson_destroy( cmd );
        return SEventsSessionInfo();
    }

    // fill array with session numbers
    bson_iter_t iter;
    bson_iter_t arrayIter;

    if( ! (bson_iter_init_find( & iter, & reply, "values")
            && BSON_ITER_HOLDS_ARRAY( & iter )
            && bson_iter_recurse( & iter, & arrayIter ))
      ){
        VS_LOG_ERROR << PRINT_HEADER << "TODO: print" << endl;
        return SEventsSessionInfo();
    }

    // get session
    vector<TSessionNum> sessionNumbers;
    while( bson_iter_next( & arrayIter ) ){
        if( BSON_ITER_HOLDS_INT32( & arrayIter ) ){
            sessionNumbers.push_back( bson_iter_int32( & arrayIter ) );
        }
    }

    bson_destroy( cmd );
    bson_destroy( & reply );
    return getSessionInfo( _persId, * std::min_element(sessionNumbers.begin(), sessionNumbers.end()) );
}

common_types::SEventsSessionInfo DatabaseManagerBase::scanPayloadTailForSessions( const common_types::TPersistenceSetId _persId ){

    const string tableName = getTableName(_persId);
    bson_t * cmd = BCON_NEW(    "distinct", BCON_UTF8( tableName.c_str() ),
                                "key", BCON_UTF8( mongo_fields::analytic::detected_object::SESSION.c_str() )
                            );

    bson_t reply;
    bson_error_t error;
    const bool rt = mongoc_database_command_simple( m_mongoDatabase,
                                                    cmd,
                                                    NULL,
                                                    & reply,
                                                    & error );

    if( 0 == rt ){
        VS_LOG_ERROR << PRINT_HEADER << " scanPayloadTailForSessions failed, reason: " << error.message << endl;
        bson_destroy( cmd );
        return SEventsSessionInfo();
    }

    // fill array with session numbers
    bson_iter_t iter;
    bson_iter_t arrayIter;

    if( ! (bson_iter_init_find( & iter, & reply, "values")
            && BSON_ITER_HOLDS_ARRAY( & iter )
            && bson_iter_recurse( & iter, & arrayIter ))
      ){
        VS_LOG_ERROR << PRINT_HEADER << "TODO: print" << endl;
        return SEventsSessionInfo();
    }

    // get session
    vector<TSessionNum> sessionNumbers;
    while( bson_iter_next( & arrayIter ) ){
        if( BSON_ITER_HOLDS_INT32( & arrayIter ) ){
            sessionNumbers.push_back( bson_iter_int32( & arrayIter ) );
        }
    }

    bson_destroy( cmd );
    bson_destroy( & reply );
    return getSessionInfo( _persId, * std::max_element(sessionNumbers.begin(), sessionNumbers.end()) );
}

vector<SEventsSessionInfo> DatabaseManagerBase::scanPayloadRangeForSessions( const TPersistenceSetId _persId,
        const std::pair<TSessionNum, TSessionNum> _sessionRange ){

    const string tableName = getTableName(_persId);
    bson_t * cmd = BCON_NEW(    "distinct", BCON_UTF8( tableName.c_str() ),
                                "key", BCON_UTF8( mongo_fields::analytic::detected_object::SESSION.c_str() )
                            );

    bson_t reply;
    bson_error_t error;
    const bool rt = mongoc_database_command_simple( m_mongoDatabase,
                                                    cmd,
                                                    NULL,
                                                    & reply,
                                                    & error );

    if( ! rt ){
        VS_LOG_ERROR << PRINT_HEADER << " scanPayloadRangeForSessions failed, reason: " << error.message << endl;
        bson_destroy( cmd );
        return std::vector<SEventsSessionInfo>();
    }

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

            if( sessionNumber >= _sessionRange.first && sessionNumber <= _sessionRange.second ){
                const SEventsSessionInfo info = getSessionInfo( _persId, sessionNumber );
                out.push_back( info );
            }
        }
    }

    bson_destroy( cmd );
    bson_destroy( & reply );
    return out;
}

vector<SEventsSessionInfo> DatabaseManagerBase::scanPayloadForSessions( const TPersistenceSetId _persId,
                                                                        const TSessionNum _beginFromSession ){

    const string tableName = getTableName(_persId);
    bson_t * cmd = BCON_NEW(    "distinct", BCON_UTF8( tableName.c_str() ),
                                "key", BCON_UTF8( mongo_fields::analytic::detected_object::SESSION.c_str() )

                                // TODO: what wrong with this shit ?
//                                "sort", "{", "logic_time", BCON_INT32(-1), "}"
//                                "query","{", "$sort", "{", "logic_time", BCON_INT32(-1), "}", "}"
                            );

    bson_t reply;
    bson_error_t error;
    const bool rt = mongoc_database_command_simple( m_mongoDatabase,
                                                    cmd,
                                                    NULL,
                                                    & reply,
                                                    & error );

    if( 0 == rt ){
        VS_LOG_ERROR << PRINT_HEADER << " scanPayloadForSessions failed, reason: " << error.message << endl;
        bson_destroy( cmd );
        return std::vector<SEventsSessionInfo>();
    }

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

            const std::vector<SEventsSessionInfo> splittedSession = splitSessionByGaps( _persId, sessionNumber );
            out.insert( out.end(), splittedSession.begin(), splittedSession.end() );
        }
    }

    bson_destroy( cmd );
    bson_destroy( & reply );
    return out;
}

SEventsSessionInfo DatabaseManagerBase::getSessionInfo( const common_types::TPersistenceSetId _persId,
        const common_types::TSessionNum _sessionNum ){

    mongoc_collection_t * contextTable = getPayloadTableRef( _persId );
    assert( contextTable );

    SEventsSessionInfo out;
    out.number = _sessionNum;

    // get MIN value
    {
        bson_t * pipeline = BCON_NEW( "pipeline", "[",
                                      "{", "$match", "{", "session", "{", "$eq", BCON_INT32(_sessionNum), "}", "}", "}",
                                      "{", "$group", "{", "_id", "$logic_time", "maxAstroTime", "{", "$max", "$astro_time", "}", "}", "}",
                                      "{", "$project", "{", "_id", BCON_INT32(1), "maxAstroTime", BCON_INT32(1), "}", "}",
                                      "{", "$sort", "{", "_id", BCON_INT32(-1), "}", "}",
                                      "]"
                                    );

        mongoc_cursor_t * cursor = mongoc_collection_aggregate( contextTable,
                                                                MONGOC_QUERY_NONE,
                                                                pipeline,
                                                                nullptr,
                                                                nullptr );
        const bson_t * doc;
        while( mongoc_cursor_next( cursor, & doc ) ){
            bson_iter_t iter;

            bson_iter_init_find( & iter, doc, "_id" );
            out.minLogicStep = bson_iter_int64( & iter );
            bson_iter_init_find( & iter, doc, "maxAstroTime" );
            out.minTimestampMillisec = bson_iter_int64( & iter );
        }

        bson_destroy( pipeline );
        mongoc_cursor_destroy( cursor );
    }

    // get MAX value
    {
        bson_t * pipeline = BCON_NEW( "pipeline", "[",
                                      "{", "$match", "{", "session", "{", "$eq", BCON_INT32(_sessionNum), "}", "}", "}",
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
        const bson_t * doc;
        while( mongoc_cursor_next( cursor, & doc ) ){
            bson_iter_t iter;

            bson_iter_init_find( & iter, doc, "_id" );
            out.maxLogicStep = bson_iter_int64( & iter );
            bson_iter_init_find( & iter, doc, "maxAstroTime" );
            out.maxTimestampMillisec = bson_iter_int64( & iter );
        }

        bson_destroy( pipeline );
        mongoc_cursor_destroy( cursor );
    }

    return out;
}

vector<SEventsSessionInfo> DatabaseManagerBase::splitSessionByGaps( const TPersistenceSetId _persId,
                                                                    const TSessionNum _sessionNum,
                                                                    const TLogicStep _logicStepThreshold ){

    mongoc_collection_t * contextTable = getPayloadTableRef( _persId );
    assert( contextTable );

    bson_t * pipeline = BCON_NEW( "pipeline", "[",
                                  "{", "$match", "{", "session", "{", "$eq", BCON_INT32(_sessionNum), "}", "}", "}",
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
    std::vector<SEventsSessionInfo> out;

    SEventsSessionInfo session;
    bool sessionOpened = false;
    int64_t previousStep = 0;

    const bson_t * doc;
    while( mongoc_cursor_next( cursor, & doc ) ){
        bson_iter_t iter;

        bson_iter_init_find( & iter, doc, "_id" );
        const TLogicStep logicStep = bson_iter_int64( & iter );

        // continue
        if( sessionOpened ){
            // OK
            if( logicStep == (previousStep + 1)  ){
                previousStep = logicStep;
                continue;
            }
            // gap is catched
            else{
                session.maxLogicStep = logicStep;
                bson_iter_init_find( & iter, doc, "maxAstroTime" );
                session.maxTimestampMillisec = bson_iter_int64( & iter );
                session.number = _sessionNum;
                out.push_back( session );

                session.clear();

                sessionOpened = false;
                previousStep = logicStep;
            }
        }
        // open
        else{
            session.minLogicStep = logicStep;
            previousStep = logicStep;

            bson_iter_init_find( & iter, doc, "maxAstroTime" );
            session.minTimestampMillisec = bson_iter_int64( & iter );
        }
    }

    bson_destroy( pipeline );
    mongoc_cursor_destroy( cursor );

    return out;
}

bool DatabaseManagerBase::isSessionExistInDescription( const common_types::TSessionNum _sessionNum ){

    bson_t * query = BCON_NEW( mongo_fields::persistence_set_description::SESSION_NUM.c_str(), BCON_INT32( _sessionNum ) );

    mongoc_cursor_t * cursor = mongoc_collection_find(  m_tablePersistenceDescription,
                                                        MONGOC_QUERY_NONE,
                                                        0,
                                                        0,
                                                        1000000, // 10000 ~= inf
                                                        query,
                                                        nullptr,
                                                        nullptr );

    const bson_t * doc;
    const bool rt = mongoc_cursor_next( cursor, & doc );
    return rt;
}

void DatabaseManagerBase::deleteSessionDescription( const common_types::TPersistenceSetId _persId, const TSessionNum _sessionNum ){

    bson_t * query = nullptr;
    if( common_vars::ALL_SESSION_NUM == _sessionNum ){
        query = BCON_NEW( mongo_fields::persistence_set_description::PERSISTENCE_ID.c_str(), BCON_INT64(_persId) );
    }
    else{
        query = BCON_NEW( "$and", "[", "{", mongo_fields::persistence_set_description::PERSISTENCE_ID.c_str(), BCON_INT64(_persId), "}",
                                       "{", mongo_fields::persistence_set_description::SESSION_NUM.c_str(), "{", "$eq", BCON_INT32(_sessionNum), "}", "}",
                                  "]"
                        );
    }

    bson_error_t error;
    const bool result = mongoc_collection_remove( m_tablePersistenceDescription, MONGOC_REMOVE_NONE, query, nullptr, & error );
    if( ! result ){
        VS_LOG_ERROR << PRINT_HEADER << " delete session descr failed, reason: " << error.message << endl;
    }

    bson_destroy( query );
}

void DatabaseManagerBase::deleteSessionDescription( const common_types::TContextId _ctxId ){

    // get persistenceId of this contextId
    const vector<SPersistenceMetadata> metadatas = getPersistenceSetMetadata( _ctxId );
    for( const SPersistenceMetadata & meta : metadatas ){
        for( const SPersistenceMetadataDSS & metaDSS : meta.persistenceFromDSS ){
            deleteSessionDescription( metaDSS.persistenceSetId );
        }

        for( const SPersistenceMetadataRaw & metaRaw : meta.persistenceFromRaw ){
            deleteSessionDescription( metaRaw.persistenceSetId );
        }

        for( const SPersistenceMetadataVideo & metaVideo : meta.persistenceFromVideo ){
            deleteSessionDescription( metaVideo.persistenceSetId );
        }
    }
}

std::vector<SEventsSessionInfo> DatabaseManagerBase::getPersistenceSetSessions( TPersistenceSetId _persId ){

    const string tableName = getTableName(_persId);
    bson_t * cmd = BCON_NEW(    "distinct", BCON_UTF8( tableName.c_str() ),
                                "key", BCON_UTF8( mongo_fields::analytic::detected_object::SESSION.c_str() )

                                // TODO: what wrong with this shit ?
//                                "sort", "{", "logic_time", BCON_INT32(-1), "}"
//                                "query","{", "$sort", "{", "logic_time", BCON_INT32(-1), "}", "}"
                            );

    bson_t reply;
    bson_error_t error;
    const bool rt = mongoc_database_command_simple( m_mongoDatabase,
                                                    cmd,
                                                    NULL,
                                                    & reply,
                                                    & error );

    if( 0 == rt ){
        VS_LOG_ERROR << PRINT_HEADER << " getPersistenceSetSessions failed, reason: " << error.message << endl;
        bson_destroy( cmd );
        return std::vector<SEventsSessionInfo>();
    }

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

    std::sort( out.begin(), out.end() );
    return out;
}

std::vector<SObjectStep> DatabaseManagerBase::getSessionSteps( TPersistenceSetId _persId, TSessionNum _sesNum ){

    mongoc_collection_t * contextTable = getPayloadTableRef( _persId );
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

// wal operations
bool DatabaseManagerBase::writeClientOperation( const common_types::SWALClientOperation & _operation ){

    bson_t * query = BCON_NEW( mongo_fields::wal_client_operations::UNIQUE_KEY.c_str(), BCON_UTF8( _operation.uniqueKey.c_str() ) );
    bson_t * update = BCON_NEW( "$set", "{",
                             mongo_fields::wal_client_operations::BEGIN.c_str(), BCON_BOOL( _operation.begin ),
                             mongo_fields::wal_client_operations::UNIQUE_KEY.c_str(), BCON_UTF8( _operation.uniqueKey.c_str() ),
                             mongo_fields::wal_client_operations::FULL_TEXT.c_str(), BCON_UTF8( _operation.commandFullText.c_str() ),
                            "}" );

    bson_error_t error;
    const bool rt = mongoc_collection_update( m_tableWALClientOperations,
                                  MONGOC_UPDATE_UPSERT,
                                  query,
                                  update,
                                  NULL,
                                  & error );

    if( ! rt ){
        VS_LOG_ERROR << PRINT_HEADER << " client operation write failed, reason: " << error.message << endl;
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

// wal system processes
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

// wal users
bool DatabaseManagerBase::writeUserRegistration( const common_types::SWALUserRegistration & _registration ){

    bson_t * doc = BCON_NEW( mongo_fields::wal_user_registrations::USER_ID.c_str(), BCON_UTF8( _registration.registerId.c_str() ),
                             mongo_fields::wal_user_registrations::USER_IP.c_str(), BCON_UTF8( _registration.userIp.c_str() ),
                             mongo_fields::wal_user_registrations::USER_PID.c_str(), BCON_INT32( _registration.userPid ),
                             mongo_fields::wal_user_registrations::REGISTERED_AT_TIME_MILLISEC.c_str(), BCON_UTF8( _registration.registeredAtDateTime.c_str() )
                           );

    bson_error_t error;
    const bool rt = mongoc_collection_insert( m_tableWALUserRegistrations,
                                              MONGOC_INSERT_NONE,
                                              doc,
                                              NULL,
                                              & error );

    if( 0 == rt ){
        VS_LOG_ERROR << PRINT_HEADER << " user registration write failed, reason: " << error.message << endl;
        bson_destroy( doc );
        return false;
    }

    bson_destroy( doc );
    return true;
}

std::vector<common_types::SWALUserRegistration> DatabaseManagerBase::getUserRegistrations(){

    bson_t * query = BCON_NEW( nullptr );
    mongoc_cursor_t * cursor = mongoc_collection_find(  m_tableWALUserRegistrations,
                                                        MONGOC_QUERY_NONE,
                                                        0,
                                                        0,
                                                        1000000, // 10000 ~= inf
                                                        query,
                                                        nullptr,
                                                        nullptr );

    std::vector<common_types::SWALUserRegistration> out;
    const bson_t * doc;
    while( mongoc_cursor_next( cursor, & doc ) ){
        uint len;
        bson_iter_t iter;

        common_types::SWALUserRegistration userReg;
        bson_iter_init_find( & iter, doc, mongo_fields::wal_user_registrations::USER_ID.c_str() );
        userReg.registerId = bson_iter_utf8( & iter, & len );
        bson_iter_init_find( & iter, doc, mongo_fields::wal_user_registrations::USER_IP.c_str() );
        userReg.userIp = bson_iter_utf8( & iter, & len );
        bson_iter_init_find( & iter, doc, mongo_fields::wal_user_registrations::USER_PID.c_str() );
        userReg.userPid = bson_iter_int32( & iter );
        bson_iter_init_find( & iter, doc, mongo_fields::wal_user_registrations::REGISTERED_AT_TIME_MILLISEC.c_str() );
        userReg.registeredAtDateTime = bson_iter_utf8( & iter, & len );

        out.push_back( userReg );
    }

    mongoc_cursor_destroy( cursor );
    bson_destroy( query );

    return out;
}

void DatabaseManagerBase::removeUserRegistration( common_types::SWALUserRegistration::TRegisterId _id ){

    bson_t * query = nullptr;
    if( ALL_REGISTRATION_IDS == _id ){
        query = BCON_NEW( nullptr );
    }
    else{
        query = BCON_NEW( mongo_fields::wal_user_registrations::USER_ID.c_str(), BCON_UTF8( _id.c_str() ));
    }

    const bool result = mongoc_collection_remove( m_tableWALUserRegistrations, MONGOC_REMOVE_NONE, query, nullptr, nullptr );
    if( ! result ){
        // TODO: do
    }

    bson_destroy( query );
}






#ifndef DATABASE_MANAGER_BASE_H
#define DATABASE_MANAGER_BASE_H

#include <unordered_map>

#include <mongoc.h>

#include "common/ms_common_types.h"
#include "common/ms_common_vars.h"

class DatabaseManagerBase
{
    static bool m_systemInited;
    static int m_instanceCounter;

    // TODO: move away from database environment
    static const std::string ALL_CLIENT_OPERATIONS;
    static const common_types::TPid ALL_PROCESS_EVENTS;
    static const std::string ALL_REGISTRATION_IDS;
public:
    struct SInitSettings {
        SInitSettings() :
            port(MONGOC_DEFAULT_PORT)
        {}
        std::string host;
        uint16_t port;
        std::string databaseName;
    };

    static DatabaseManagerBase * getInstance();
    static void destroyInstance( DatabaseManagerBase * & _inst );

    bool init( SInitSettings _settings );

    // object payload
    bool writeTrajectoryData( common_types::TPersistenceSetId _persId, const std::vector<common_types::SPersistenceTrajectory> & _data );
    std::vector<common_types::SPersistenceTrajectory> readTrajectoryData( const common_types::SPersistenceSetFilter & _filter );
    bool writeWeatherData( common_types::TPersistenceSetId _persId, const std::vector<common_types::SPersistenceWeather> & _data );
    std::vector<common_types::SPersistenceWeather> readWeatherData( const common_types::SPersistenceSetFilter & _filter );

    void removeTotalData( const common_types::SPersistenceSetFilter & _filter );

    // object payload - metadata about datasources list
    common_types::TPersistenceSetId writePersistenceSetMetadata( const common_types::SPersistenceMetadataVideo & _videoMetadata );
    common_types::TPersistenceSetId writePersistenceSetMetadata( const common_types::SPersistenceMetadataDSS & _type );
    common_types::TPersistenceSetId writePersistenceSetMetadata( const common_types::SPersistenceMetadataRaw & _rawMetadata );
    std::vector<common_types::SPersistenceMetadata> getPersistenceSetMetadata( common_types::TContextId _ctxId = common_vars::ALL_CONTEXT_ID );
    std::vector<common_types::SPersistenceMetadata> getPersistenceSetMetadata( common_types::TPersistenceSetId _persId );
    void removePersistenceSetMetadata( common_types::TPersistenceSetId _id );
    void removePersistenceSetMetadata( common_types::TContextId _ctxId );

    // object payload - metadata about specific datasource
    std::vector<common_types::SEventsSessionInfo> getPersistenceSetSessions( common_types::TPersistenceSetId _persId );
    std::vector<common_types::SObjectStep> getSessionSteps( common_types::TPersistenceSetId _persId, common_types::TSessionNum _sesNum );

    // WAL
    bool writeClientOperation( const common_types::SWALClientOperation & _operation );
    std::vector<common_types::SWALClientOperation> getClientOperations();
    std::vector<common_types::SWALClientOperation> getNonIntegrityClientOperations();
    void removeClientOperation( std::string _uniqueKey = ALL_CLIENT_OPERATIONS );    

    bool writeProcessEvent( const common_types::SWALProcessEvent & _event, bool _launch );
    std::vector<common_types::SWALProcessEvent> getProcessEvents( common_types::TPid _pid = ALL_PROCESS_EVENTS );
    std::vector<common_types::SWALProcessEvent> getNonIntegrityProcessEvents();
    void removeProcessEvent( common_types::TPid _pid = ALL_PROCESS_EVENTS );

    bool writeUserRegistration( const common_types::SWALUserRegistration & _registration );
    std::vector<common_types::SWALUserRegistration> getUserRegistrations();
    void removeUserRegistration( std::string _registrationId = ALL_REGISTRATION_IDS );


private:
    static void systemInit();

    DatabaseManagerBase();
    ~DatabaseManagerBase();

    DatabaseManagerBase( const DatabaseManagerBase & _inst ) = delete;
    DatabaseManagerBase & operator=( const DatabaseManagerBase & _inst ) = delete;

    inline bool createIndex( const std::string & _tableName, const std::vector<std::string> & _fieldNames );
    inline mongoc_collection_t * getAnalyticContextTable( common_types::TPersistenceSetId _persId );
    inline std::string getTableName( common_types::TPersistenceSetId _persId );

    void writePersistenceMetadataGlobal( common_types::TPersistenceSetId _persId, const common_types::SPersistenceMetadataDescr & _meta );
    void writePersistenceFromVideo( const common_types::SPersistenceMetadataVideo & _videoMetadata );
    void writePersistenceFromDSS( const common_types::SPersistenceMetadataDSS & _type );
    void writePersistenceFromRaw( const common_types::SPersistenceMetadataRaw & _type );

    bool getPersistenceFromVideo( common_types::TPersistenceSetId _persId, common_types::SPersistenceMetadataVideo & _meta );
    bool getPersistenceFromDSS( common_types::TPersistenceSetId _persId, common_types::SPersistenceMetadataDSS & _meta );
    bool getPersistenceFromRaw( common_types::TPersistenceSetId _persId, common_types::SPersistenceMetadataRaw & _meta );

    bool isPersistenceMetadataValid( common_types::TPersistenceSetId _persId, const common_types::SPersistenceMetadataDescr & _meta );
    common_types::TPersistenceSetId createNewPersistenceId();

    // data
    mongoc_collection_t * m_tableWALClientOperations;
    mongoc_collection_t * m_tableWALProcessEvents;
    mongoc_collection_t * m_tableWALUserRegistrations;
    mongoc_collection_t * m_tablePersistenceDescr;
    mongoc_collection_t * m_tablePersistenceFromVideo;
    mongoc_collection_t * m_tablePersistenceFromDSS;
    mongoc_collection_t * m_tablePersistenceFromRaw;
    std::vector<mongoc_collection_t *> m_allCollections;
    std::unordered_map<common_types::TContextId, mongoc_collection_t *> m_contextCollections;
    SInitSettings m_settings;

    // service
    mongoc_client_t * m_mongoClient;
    mongoc_database_t * m_database;
};

#endif // DATABASE_MANAGER_BASE_H


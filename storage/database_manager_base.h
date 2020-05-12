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
        SInitSettings()
            : host("localhost")
            , port(MONGOC_DEFAULT_PORT)
        {}
        std::string host;
        uint16_t port;
        std::string databaseName;
        std::string projectPrefix;
    };

    static DatabaseManagerBase * getInstance();
    static void destroyInstance( DatabaseManagerBase * & _inst );

    bool init( SInitSettings _settings );

    // -------------------------------------------------------------------------------------
    // objects ( by 'pers id' -> more precise approach, while by 'ctx id' -> more global
    // -------------------------------------------------------------------------------------
    // metadata
    common_types::TPersistenceSetId writePersistenceSetMetadata( const common_types::SPersistenceMetadataVideo & _videoMetadata );
    common_types::TPersistenceSetId writePersistenceSetMetadata( const common_types::SPersistenceMetadataDSS & _dssMetadata );
    common_types::TPersistenceSetId writePersistenceSetMetadata( const common_types::SPersistenceMetadataRaw & _rawMetadata );
    std::vector<common_types::SPersistenceMetadata> getPersistenceSetMetadata( common_types::TContextId _ctxId = common_vars::ALL_CONTEXT_ID );
    common_types::SPersistenceMetadata getPersistenceSetMetadata( common_types::TPersistenceSetId _persId );
    void deletePersistenceSetMetadata( common_types::TPersistenceSetId _id );
    void deletePersistenceSetMetadata( common_types::TContextId _ctxId );

    // payload
    bool writeTrajectoryData( common_types::TPersistenceSetId _persId, const std::vector<common_types::SPersistenceTrajectory> & _data );
    std::vector<common_types::SPersistenceTrajectory> readTrajectoryData( const common_types::SPersistenceSetFilter & _filter );
    bool writeWeatherData( common_types::TPersistenceSetId _persId, const std::vector<common_types::SPersistenceWeather> & _data );
    std::vector<common_types::SPersistenceWeather> readWeatherData( const common_types::SPersistenceSetFilter & _filter );
    void deleteTotalData( const common_types::SPersistenceSetFilter & _filter );
    void deleteTotalData( const common_types::TContextId _ctxId );

    // payload description
    bool insertSessionDescription( const common_types::TPersistenceSetId _persId, const common_types::SEventsSessionInfo & _descr );
    bool updateSessionDescription( const common_types::TPersistenceSetId _persId, const common_types::SEventsSessionInfo & _descr );
    std::vector<common_types::SEventsSessionInfo> selectSessionDescriptions( const common_types::TPersistenceSetId _persId );
    std::vector<common_types::SEventsSessionInfo> scanPayloadForSessions( const common_types::TPersistenceSetId _persId,
                                                                          const common_types::TSessionNum _beginFromSession = 0 );
    std::vector<common_types::SEventsSessionInfo> scanPayloadForSessions2( const common_types::TPersistenceSetId _persId,
                                                                           const common_types::TSessionNum _beginFromSession = 0 );
    void deleteSessionDescription( const common_types::TPersistenceSetId _persId, const common_types::TSessionNum _sessionNum = common_vars::ALL_SESSION_NUM );
    void deleteSessionDescription( const common_types::TContextId _ctxId );

    // TODO: obsoleted ( heavy version )
    std::vector<common_types::SEventsSessionInfo> getPersistenceSetSessions( common_types::TPersistenceSetId _persId );
    std::vector<common_types::SObjectStep> getSessionSteps( common_types::TPersistenceSetId _persId, common_types::TSessionNum _sesNum );

    // -------------------------------------------------------------------------------------
    // WAL
    // -------------------------------------------------------------------------------------
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

    // object payload - metadata
    void writePersistenceMetadataGlobal( const common_types::TPersistenceSetId _persId, const std::string _payloadTableName, const common_types::SPersistenceMetadataDescr & _meta );
    void writePersistenceFromVideo( const common_types::SPersistenceMetadataVideo & _videoMetadata );
    void writePersistenceFromDSS( const common_types::SPersistenceMetadataDSS & _type );
    void writePersistenceFromRaw( const common_types::SPersistenceMetadataRaw & _type );

    bool getPersistenceFromVideo( common_types::TPersistenceSetId _persId, common_types::SPersistenceMetadataVideo & _meta );
    bool getPersistenceFromDSS( common_types::TPersistenceSetId _persId, common_types::SPersistenceMetadataDSS & _meta );
    bool getPersistenceFromRaw( common_types::TPersistenceSetId _persId, common_types::SPersistenceMetadataRaw & _meta );

    void deletePersistenceFromRaw( common_types::TPersistenceSetId _persId );
    void deletePersistenceFromDSS( common_types::TPersistenceSetId _persId );
    void deletePersistenceFromVideo( common_types::TPersistenceSetId _persId );

    // object payload - description
    bool isSessionExistInDescription( const common_types::TSessionNum _sessionNum );
    common_types::SEventsSessionInfo getSessionInfo( const common_types::TPersistenceSetId _persId,
            const common_types::TSessionNum _sessionNum );
    std::vector<common_types::SEventsSessionInfo> splitSessionByGaps( const common_types::TPersistenceSetId _persId,
                                                                      const common_types::TSessionNum _sessionNum,
                                                                      const common_types::TLogicStep _logicStepThreshold = 0 );

    // service
    inline void createPayloadTableRef( common_types::TPersistenceSetId _persId, const std::string _tableName );
    inline mongoc_collection_t * getPayloadTableRef( common_types::TPersistenceSetId _persId );
    inline std::string getTableName( common_types::TPersistenceSetId _persId );
    inline bool createIndex( const std::string & _tableName, const std::vector<std::string> & _fieldNames );

    bool isPersistenceMetadataValid( common_types::TPersistenceSetId _persId, const common_types::SPersistenceMetadataDescr & _meta );
    common_types::TPersistenceSetId createNewPersistenceId();

    // data
    SInitSettings m_settings;
    mongoc_collection_t * m_tableWALClientOperations;
    mongoc_collection_t * m_tableWALProcessEvents;
    mongoc_collection_t * m_tableWALUserRegistrations;
    mongoc_collection_t * m_tablePersistenceMetadata;
    mongoc_collection_t * m_tablePersistenceDescription;
    mongoc_collection_t * m_tablePersistenceFromVideo;
    mongoc_collection_t * m_tablePersistenceFromDSS;
    mongoc_collection_t * m_tablePersistenceFromRaw;
    std::vector<mongoc_collection_t *> m_allTables;
    std::unordered_map<common_types::TPersistenceSetId, mongoc_collection_t *> m_tablesByPersistenceId;
    std::unordered_map<common_types::TPersistenceSetId, std::string> m_tableNameByPersistenceId;
    std::string m_tableNamePrefix;

    // service
    mongoc_client_t * m_mongoClient;
    mongoc_database_t * m_mongoDatabase;
};

#endif // DATABASE_MANAGER_BASE_H


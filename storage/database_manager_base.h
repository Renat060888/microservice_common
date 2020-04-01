#ifndef DATABASE_MANAGER_BASE_H
#define DATABASE_MANAGER_BASE_H

#include <unordered_map>

#include <mongoc.h>

#include "common/ms_common_types.h"

class DatabaseManagerBase
{
    static bool m_systemInited;
    static int m_instanceCounter;
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

    static DatabaseManagerBase * getInstance(){
        if( ! m_systemInited ){
            systemInit();
            m_systemInited = true;
        }
        m_instanceCounter++;
        return new DatabaseManagerBase();
    }

    static void destroyInstance( DatabaseManagerBase * & _inst ){
        delete _inst;
        _inst = nullptr;
        m_instanceCounter--;
    }

    bool init( SInitSettings _settings );

    // analyze - data
    bool writeTrajectoryData( common_types::TPersistenceSetId _persId, const std::vector<common_types::SPersistenceTrajectory> & _data );
    std::vector<common_types::SPersistenceTrajectory> readTrajectoryData( const common_types::SPersistenceSetFilter & _filter );
    bool writeWeatherData( common_types::TPersistenceSetId _persId, const std::vector<common_types::SPersistenceWeather> & _data );
    std::vector<common_types::SPersistenceWeather> readWeatherData( const common_types::SPersistenceSetFilter & _filter );

    void removeTotalData( const common_types::SPersistenceSetFilter & _filter );

    // analyze - metadata about datasources
    common_types::TPersistenceSetId writePersistenceSetMetadata( const common_types::SPersistenceMetadataVideo & _type );
    common_types::TPersistenceSetId writePersistenceSetMetadata( const common_types::SPersistenceMetadataDSS & _type );
    common_types::TPersistenceSetId writePersistenceSetMetadata( const common_types::SPersistenceMetadataRaw & _type );
    common_types::SPersistenceMetadata getPersistenceSetMetadata( common_types::TContextId _ctxId );
    void removePersistenceSetMetadata( common_types::TPersistenceSetId _id );

    // analyze - metadata about specific datasource
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

    // data
    mongoc_collection_t * m_tableWALClientOperations;
    mongoc_collection_t * m_tableWALProcessEvents;    
    std::vector<mongoc_collection_t *> m_allCollections;
    std::unordered_map<common_types::TContextId, mongoc_collection_t *> m_contextCollections;
    SInitSettings m_settings;

    // service
    mongoc_client_t * m_mongoClient;
    mongoc_database_t * m_database;
};

#endif // DATABASE_MANAGER_BASE_H


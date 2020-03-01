#ifndef DATABASE_MANAGER_ASTRA_H
#define DATABASE_MANAGER_ASTRA_H

#include <unordered_map>

#include <mongoc.h>
#include "common/ms_common_types.h"

class DatabaseManager
{
    static bool m_systemInited;
    static int m_instanceCounter;
    static const std::string ALL_CLIENT_OPERATIONS;
    static const common_types::TPid ALL_PROCESS_EVENTS;
public:
    struct SInitSettings {
        SInitSettings() :
            port(MONGOC_DEFAULT_PORT)
        {}
        std::string host;
        uint16_t port;
        std::string databaseName;
    };

    static DatabaseManager * getInstance(){
        if( ! m_systemInited ){
            systemInit();
            m_systemInited = true;
        }
        m_instanceCounter++;
        return new DatabaseManager();
    }

    static void destroyInstance( DatabaseManager * & _inst ){
        delete _inst;
        _inst = nullptr;
        m_instanceCounter--;
    }

    bool init( SInitSettings _settings );

    // WAL
    bool writeClientOperation( const common_types::SWALClientOperation & _operation );
    std::vector<common_types::SWALClientOperation> getClientOperations();
    std::vector<common_types::SWALClientOperation> getNonIntegrityClientOperations();
    void removeClientOperation( std::string _uniqueKey = ALL_CLIENT_OPERATIONS );    

    bool writeProcessEvent( const common_types::SWALProcessEvent & _event, bool _launch );
    std::vector<common_types::SWALProcessEvent> getProcessEvents( common_types::TPid _pid = ALL_PROCESS_EVENTS );
    std::vector<common_types::SWALProcessEvent> getNonIntegrityProcessEvents();
    void removeProcessEvent( common_types::TPid _pid = ALL_PROCESS_EVENTS );


private:
    static void systemInit();

    DatabaseManager();
    ~DatabaseManager();

    DatabaseManager( const DatabaseManager & _inst ) = delete;
    DatabaseManager & operator=( const DatabaseManager & _inst ) = delete;

    inline mongoc_collection_t * getAnalyticContextTable( common_types::TContextId _ctxId, TSensorId _sensorId );
    inline std::string getTableName( common_types::TContextId _ctxId, TSensorId _sensorId );
    inline bool createIndex( const std::string & _tableName, const std::vector<std::string> & _fieldNames );

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

#endif // DATABASE_MANAGER_ASTRA_H


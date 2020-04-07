
// add GMock library

#include "storage/database_manager_base.h"
#include "storage_tests.h"

using namespace std;

StorageTests::StorageTests()
{

}

bool StorageTests::runTests(){

    // init
    DatabaseManagerBase::SInitSettings settings;
    settings.host = "localhost";
    settings.databaseName = "recorder_agent";

    DatabaseManagerBase * db = DatabaseManagerBase::getInstance();
    if( ! db->init(settings) ){
        return false;
    }

    // test write
    common_types::SPersistenceMetadataRaw videoMetadata;
    videoMetadata.contextId = 777;
    videoMetadata.missionId = 0;
    videoMetadata.lastRecordedSession = 1;
    videoMetadata.sourceType = common_types::EPersistenceSourceType::AUTONOMOUS_RECORDER;
    videoMetadata.timeStepIntervalMillisec = 100;

    const common_types::TPersistenceSetId persId = db->writePersistenceSetMetadata( videoMetadata );

    assert( persId != common_types::SPersistenceMetadataDescr::INVALID_PERSISTENCE_ID );


    // test read




    // test delete






    return true;
}



































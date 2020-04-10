
#include <gtest/gtest.h>

#include "storage/database_manager_base.h"
#include "storage_tests.h"

using namespace std;

// -----------------------------------------------------------------------------------
// Google Test Example
// -----------------------------------------------------------------------------------

// The fixture for testing class Foo.
class GoogleTestExample : public ::testing::Test {
protected:
    // You can remove any or all of the following functions if their bodies would be empty.

    GoogleTestExample() {
        // You can do set-up work for each test here.
    }

    ~GoogleTestExample() override {
        // You can do clean-up work that doesn't throw exceptions here.
    }

    // If the constructor and destructor are not enough for setting up
    // and cleaning up each test, you can define the following methods:

    void SetUp() override {
        // Code here will be called immediately after the constructor (right
        // before each test).
    }

    void TearDown() override {
        // Code here will be called immediately after each test (right
        // before the destructor).
    }

    // Class members declared here can be used by all tests in the test suite for Foo.
};


StorageTests::StorageTests()
{

}

bool StorageTests::runTests(){

    constexpr common_types::TContextId CTX_ID = 777;
    constexpr common_types::TMissionId MISSION_ID = 0;
    constexpr int64_t UPDATE_MILLISEC = 100;

    // init
    DatabaseManagerBase::SInitSettings settings;
    settings.host = "localhost";
    settings.databaseName = "recorder_agent";

    DatabaseManagerBase * db = DatabaseManagerBase::getInstance();
    if( ! db->init(settings) ){
        return false;
    }

    // test read
    common_types::TPersistenceSetId persId = common_types::SPersistenceMetadataDescr::INVALID_PERSISTENCE_ID;
    common_types::TSessionNum previousSessionNum = 0;

    const std::vector<common_types::SPersistenceMetadata> previousMetas = db->getPersistenceSetMetadata( (common_types::TContextId)777 );
    if( ! previousMetas.empty() ){
        const common_types::SPersistenceMetadata & previousMeta = previousMetas.front();
        const common_types::SPersistenceMetadataRaw rawMeta = previousMeta.persistenceFromRaw[ 0 ];
        persId = rawMeta.persistenceSetId;
        previousSessionNum = rawMeta.lastRecordedSession;
    }

    // test write
    common_types::SPersistenceMetadataRaw rawMetadata;
    rawMetadata.persistenceSetId = persId;
    rawMetadata.contextId = CTX_ID;
    rawMetadata.missionId = MISSION_ID;
    rawMetadata.lastRecordedSession = ++previousSessionNum;
    rawMetadata.sourceType = common_types::EPersistenceSourceType::AUTONOMOUS_RECORDER;
    rawMetadata.timeStepIntervalMillisec = UPDATE_MILLISEC;

    persId = db->writePersistenceSetMetadata( rawMetadata );

    assert( persId != common_types::SPersistenceMetadataDescr::INVALID_PERSISTENCE_ID );


    // test delete






    return true;
}



































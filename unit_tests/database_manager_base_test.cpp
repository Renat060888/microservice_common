
#include <microservice_common/system/logger.h>

#include "database_manager_base_test.h"

using namespace std;
using namespace common_types;

static const TContextId CONTEXT_ID = 777;
static const TMissionId MISSION_ID = 555;
static const int64_t QUANTUM_INTERVAL_MILLISEC = 1000;

DatabaseManagerBase * DatabaseManagerBaseTest::m_database = nullptr;

DatabaseManagerBaseTest::DatabaseManagerBaseTest()
{


}

void DatabaseManagerBaseTest::SetUpTestCase(){

    m_database = DatabaseManagerBase::getInstance();

    DatabaseManagerBase::SInitSettings settings;
    settings.host = "localhost";
    settings.databaseName = "unit_tests";

    // TODO: check with assert also
    m_database->init(settings);
}

void DatabaseManagerBaseTest::TearDownTestCase(){

    DatabaseManagerBase::destroyInstance( m_database );
}

// -------------------------------------------------------------------------
// object trajectory tests
// -------------------------------------------------------------------------
TEST_F(DatabaseManagerBaseTest, DISABLED_metadata_test_recorder){

    // I check for correct clearing
    {
        m_database->deleteTotalData( CONTEXT_ID );
        m_database->deleteSessionDescription( CONTEXT_ID );
        m_database->deletePersistenceSetMetadata( CONTEXT_ID );

        const std::vector<SPersistenceMetadata> ctxPersistenceMetadatas1 = m_database->getPersistenceSetMetadata( CONTEXT_ID );
        ASSERT_TRUE( ctxPersistenceMetadatas1.empty() );
    }

    // II write metadata common_types::SPersistenceMetadataRaw rawMetadata;
    SPersistenceMetadataRaw rawMetadataInput;
    {
        rawMetadataInput.contextId = CONTEXT_ID;
        rawMetadataInput.missionId = MISSION_ID;
        rawMetadataInput.lastRecordedSession = 1;
        rawMetadataInput.sourceType = common_types::EPersistenceSourceType::AUTONOMOUS_RECORDER;
        rawMetadataInput.timeStepIntervalMillisec = QUANTUM_INTERVAL_MILLISEC;

        const TPersistenceSetId persId = m_database->writePersistenceSetMetadata( rawMetadataInput );
        rawMetadataInput.persistenceSetId = persId;
        ASSERT_NE( persId, -1 );
    }

    // III check for correct writing
    {
        const std::vector<SPersistenceMetadata> ctxPersistenceMetadatas2 = m_database->getPersistenceSetMetadata( CONTEXT_ID );
        ASSERT_TRUE( ! ctxPersistenceMetadatas2.empty() );

        const SPersistenceMetadataRaw & rawMetadataOutput = ctxPersistenceMetadatas2[ 0 ].persistenceFromRaw.front();
        ASSERT_EQ( rawMetadataInput, rawMetadataOutput );
    }
}

TEST_F(DatabaseManagerBaseTest, payload_test_recorder){

    // clear first
    m_database->deleteTotalData( CONTEXT_ID );
    m_database->deleteSessionDescription( CONTEXT_ID );
    m_database->deletePersistenceSetMetadata( CONTEXT_ID );

    // I write metadata common_types::SPersistenceMetadataRaw rawMetadata;
    TPersistenceSetId persId = -1;
    {
        SPersistenceMetadataRaw rawMetadataInput;
        rawMetadataInput.contextId = CONTEXT_ID;
        rawMetadataInput.missionId = MISSION_ID;
        rawMetadataInput.lastRecordedSession = 1;
        rawMetadataInput.sourceType = common_types::EPersistenceSourceType::AUTONOMOUS_RECORDER;
        rawMetadataInput.timeStepIntervalMillisec = QUANTUM_INTERVAL_MILLISEC;

        persId = m_database->writePersistenceSetMetadata( rawMetadataInput );
        rawMetadataInput.persistenceSetId = persId;
        ASSERT_NE( persId, -1 );
    }

    // II write payload
    {
        vector<common_types::SPersistenceTrajectory> data;
        SPersistenceTrajectory trajInput;
        trajInput.ctxId = CONTEXT_ID;
        trajInput.missionId = MISSION_ID;
        trajInput.objId = 123;
        trajInput.state = SPersistenceObj::EState::ACTIVE;
        trajInput.latDeg = 40.0f;
        trajInput.lonDeg = 90.0f;

        // reproduce all possible cases ( '*' -> step exist, '-' -> empty cell )
        // |***** *****|***** -----|----- -----|----- *****|***** -----|---** **---|***-- --***|
        // P1 (s1)     P2          P3          P4     s2   P5          P6 (s3)     P7 (s4)  s5

        // S1
        trajInput.sessionNum = 1;
        trajInput.logicTime = -1;
        trajInput.astroTimeMillisec = 9000;

        for( int i = 0; i < 15; i++ ){
            trajInput.logicTime++;
            trajInput.latDeg += ::rand() % 10;
            trajInput.lonDeg += ::rand() % 10;
            trajInput.astroTimeMillisec += 1000;
            data.push_back( trajInput );
        }

        // S2
        trajInput.sessionNum = 2;
        trajInput.logicTime = -1;
        trajInput.astroTimeMillisec += 20000;

        for( int i = 0; i < 10; i++ ){
            trajInput.logicTime++;
            trajInput.latDeg += ::rand() % 10;
            trajInput.lonDeg += ::rand() % 10;
            trajInput.astroTimeMillisec += 1000;
            data.push_back( trajInput );
        }

        // S3
        trajInput.sessionNum = 3;
        trajInput.logicTime = -1;

        for( int i = 0; i < 4; i++ ){
            trajInput.logicTime++;
            trajInput.latDeg += ::rand() % 10;
            trajInput.lonDeg += ::rand() % 10;
            trajInput.astroTimeMillisec += 1000;
            data.push_back( trajInput );
        }

        // S4
        trajInput.sessionNum = 4;
        trajInput.logicTime = -1;
        trajInput.astroTimeMillisec += 8000;

        for( int i = 0; i < 3; i++ ){
            trajInput.logicTime++;
            trajInput.latDeg += ::rand() % 10;
            trajInput.lonDeg += ::rand() % 10;
            trajInput.astroTimeMillisec += 1000;
            data.push_back( trajInput );
        }

        // S5
        trajInput.sessionNum = 5;
        trajInput.logicTime = -1;
        trajInput.astroTimeMillisec += 3000;

        for( int i = 0; i < 3; i++ ){
            trajInput.logicTime++;
            trajInput.latDeg += ::rand() % 10;
            trajInput.lonDeg += ::rand() % 10;
            trajInput.astroTimeMillisec += 1000;
            data.push_back( trajInput );
        }

        m_database->writeTrajectoryData( persId, data );
    }

    // III check written payload ( whole area request )
    {
        SPersistenceSetFilter filter(persId);
        filter.minLogicStep = -1;
        const std::vector<SPersistenceTrajectory> trajOutput = m_database->readTrajectoryData( filter );

        SPersistenceTrajectory trajStandart;
        trajStandart.ctxId = CONTEXT_ID;
        trajStandart.missionId = MISSION_ID;
        trajStandart.objId = 123;
        trajStandart.state = SPersistenceObj::EState::ACTIVE;

        // S1
        trajStandart.sessionNum = 1;
        trajStandart.logicTime = -1;
        trajStandart.astroTimeMillisec = 9000;

        for( int i = 0; i < 15; i++ ){
            trajStandart.astroTimeMillisec += 1000;
            ASSERT_EQ( trajOutput[ i ].logicTime, ++trajStandart.logicTime );
            ASSERT_EQ( trajOutput[ i ].astroTimeMillisec, trajStandart.astroTimeMillisec );
            ASSERT_EQ( trajOutput[ i ].sessionNum, trajStandart.sessionNum );
            ASSERT_EQ( trajOutput[ i ].objId, trajStandart.objId );
            ASSERT_EQ( trajOutput[ i ].state, trajStandart.state );
        }

        // S2
        trajStandart.sessionNum = 2;
        trajStandart.logicTime = -1;
        trajStandart.astroTimeMillisec += 15000;

        for( int i = 15; i < 25; i++ ){
            trajStandart.astroTimeMillisec += 1000;
            ASSERT_EQ( trajOutput[ i ].logicTime, ++trajStandart.logicTime );
            ASSERT_EQ( trajOutput[ i ].astroTimeMillisec, trajStandart.astroTimeMillisec );
            ASSERT_EQ( trajOutput[ i ].sessionNum, trajStandart.sessionNum );
            ASSERT_EQ( trajOutput[ i ].objId, trajStandart.objId );
            ASSERT_EQ( trajOutput[ i ].state, trajStandart.state );
        }

        // S3
        trajStandart.sessionNum = 3;
        trajStandart.logicTime = -1;
        trajStandart.astroTimeMillisec += 15000;

        for( int i = 25; i < 29; i++ ){
            trajStandart.astroTimeMillisec += 1000;
            ASSERT_EQ( trajOutput[ i ].logicTime, ++trajStandart.logicTime );
            ASSERT_EQ( trajOutput[ i ].astroTimeMillisec, trajStandart.astroTimeMillisec );
            ASSERT_EQ( trajOutput[ i ].sessionNum, trajStandart.sessionNum );
            ASSERT_EQ( trajOutput[ i ].objId, trajStandart.objId );
            ASSERT_EQ( trajOutput[ i ].state, trajStandart.state );
        }

        // S4
        trajStandart.sessionNum = 4;
        trajStandart.logicTime = -1;
        trajStandart.astroTimeMillisec += 15000;

        for( int i = 29; i < 32; i++ ){
            trajStandart.astroTimeMillisec += 1000;
            ASSERT_EQ( trajOutput[ i ].logicTime, ++trajStandart.logicTime );
            ASSERT_EQ( trajOutput[ i ].astroTimeMillisec, trajStandart.astroTimeMillisec );
            ASSERT_EQ( trajOutput[ i ].sessionNum, trajStandart.sessionNum );
            ASSERT_EQ( trajOutput[ i ].objId, trajStandart.objId );
            ASSERT_EQ( trajOutput[ i ].state, trajStandart.state );
        }

        // S5
        trajStandart.sessionNum = 5;
        trajStandart.logicTime = -1;
        trajStandart.astroTimeMillisec += 15000;

        for( int i = 32; i < 35; i++ ){
            trajStandart.astroTimeMillisec += 1000;
            ASSERT_EQ( trajOutput[ i ].logicTime, ++trajStandart.logicTime );
            ASSERT_EQ( trajOutput[ i ].astroTimeMillisec, trajStandart.astroTimeMillisec );
            ASSERT_EQ( trajOutput[ i ].sessionNum, trajStandart.sessionNum );
            ASSERT_EQ( trajOutput[ i ].objId, trajStandart.objId );
            ASSERT_EQ( trajOutput[ i ].state, trajStandart.state );
        }
    }
}

// functors
struct FunctorLessSPersistenceTrajectory {
    bool operator()( const SPersistenceTrajectory & _lhs, const SPersistenceTrajectory & _rhs ){
        return ( _lhs.sessionNum < _rhs.sessionNum );
    }
};

struct FunctorEqualSPersistenceTrajectory {
    bool operator()( const SPersistenceTrajectory & _lhs, const SPersistenceTrajectory & _rhs ){
        return ( _lhs.sessionNum == _rhs.sessionNum );
    }
};
// functors

TEST_F(DatabaseManagerBaseTest, description_test_recorder){

    // NOTE: metadata & payload already written by previous test
    const std::vector<SPersistenceMetadata> ctxPersistenceMetadatas2 = m_database->getPersistenceSetMetadata( CONTEXT_ID );
    const SPersistenceMetadataRaw & rawMetadataOutput = ctxPersistenceMetadatas2[ 0 ].persistenceFromRaw.front();

    // I check delete command ( pers id )
    {
        m_database->deleteSessionDescription( CONTEXT_ID );

        const vector<SEventsSessionInfo> storedSessionInfo = m_database->selectSessionDescriptions( rawMetadataOutput.persistenceSetId );
        ASSERT_TRUE( storedSessionInfo.empty() );
    }

    // II create new description
    vector<SEventsSessionInfo> storedSessionInfo2;
    {
        const vector<SEventsSessionInfo> scannedSessionInfo = m_database->scanPayloadForSessions2( rawMetadataOutput.persistenceSetId );
        for( const SEventsSessionInfo & descr : scannedSessionInfo ){
            m_database->insertSessionDescription( rawMetadataOutput.persistenceSetId, descr );
        }

        storedSessionInfo2 = m_database->selectSessionDescriptions( rawMetadataOutput.persistenceSetId );
        ASSERT_EQ( scannedSessionInfo.size(), storedSessionInfo2.size() );
    }

    // III check description with payload for correctness
    {
        SPersistenceSetFilter filter(rawMetadataOutput.persistenceSetId);
        filter.minLogicStep = -1;
        const std::vector<SPersistenceTrajectory> trajOutput = m_database->readTrajectoryData( filter );

        // count sessions
        std::vector<SPersistenceTrajectory> trajOutputCopy = trajOutput;
        std::sort( trajOutputCopy.begin(), trajOutputCopy.end(), FunctorLessSPersistenceTrajectory() );
        auto iter = std::unique( trajOutputCopy.begin(), trajOutputCopy.end(), FunctorEqualSPersistenceTrajectory() );
        trajOutputCopy.erase( iter, trajOutputCopy.end() );

        const int payloadSessionCount = trajOutputCopy.size();
        ASSERT_EQ( payloadSessionCount, storedSessionInfo2.size() );

        // check session by session

    }

    // IV update description
    {

    }

    // add more payload
    // |***-- *****|
    // P8 (s5) s6


    return;


    const SEventsSessionInfo & lastStoredSession = storedSessionInfo2.back();
    vector<SEventsSessionInfo> scannedSessions = m_database->scanPayloadForSessions( rawMetadataOutput.persistenceSetId,
                                                                                              lastStoredSession.number );
    // compare last stored session with scanned ones ( leave only sessions that more or equal to last stored )
    for( auto iter = scannedSessions.begin(); iter != scannedSessions.end(); ){
        const SEventsSessionInfo & sesInfo = ( * iter );

        if( sesInfo.number < lastStoredSession.number ){
            scannedSessions.erase( iter );
        }
        else{
            ++iter;
        }
    }

    // update 5th session
    const SEventsSessionInfo & fifthSession = scannedSessions.front();
    m_database->updateSessionDescription( rawMetadataOutput.persistenceSetId, fifthSession );

    // add new 6th session
    const SEventsSessionInfo & newSixthSession = scannedSessions.back();
    m_database->insertSessionDescription( rawMetadataOutput.persistenceSetId, newSixthSession );


}

// -------------------------------------------------------------------------
// ... tests
// -------------------------------------------------------------------------











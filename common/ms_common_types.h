#ifndef MS_COMMON_TYPES_H
#define MS_COMMON_TYPES_H

#include <memory>
#include <vector>
#include <string>

namespace common_types {


// ---------------------------------------------------------------------------
// forwards
// ---------------------------------------------------------------------------
class IWALRecordVisitor;
class IPersistenceEntityVisitor;


// ---------------------------------------------------------------------------
// global typedefs
// ---------------------------------------------------------------------------

// system types
using TPid = pid_t;
using TCommandLineArgKey = std::string;
using TCommandLineArgVal = std::string;

// service types
using TUserId = std::string;

// objrepr types
using TContextId = uint32_t;
using TMissionId = uint32_t;
using TSensorId = uint64_t;
using TObjectId = uint64_t;

// player types
using TTimeRangeMillisec = std::pair<int64_t, int64_t>;
using TSessionNum = int32_t;
using TLogicStep = int64_t;

// database types
using TPersistenceSetId = int64_t;


// ---------------------------------------------------------------------------
// enums
// ---------------------------------------------------------------------------
enum class EServerFeatures : uint16_t {
    RETRANSLATION   = 1 << 0,
    ARCHIVING       = 1 << 1,
    ANALYZE         = 1 << 2,

    RESERVED1       = 1 << 3,
    RESERVED2       = 1 << 4,
    RESERVED3       = 1 << 5,
    RESERVED4       = 1 << 6,
    RESERVED5       = 1 << 7,

    UNDEFINED       = 1 << 15,
};

// TODO: injection implementation details to common library (!)
enum class EPersistenceSourceType {
    VIDEO_SERVER,
    DSS,
    AUTONOMOUS_RECORDER,
    UNDEFINED
};

enum class EPersistenceDataType {
    TRAJECTORY,
    WEATHER,
    UNDEFINED
};

// ---------------------------------------------------------------------------
// simple ADT
// ---------------------------------------------------------------------------
struct SPersistenceMetadataDescr {
    SPersistenceMetadataDescr()
        : persistenceSetId(INVALID_PERSISTENCE_ID)
        , contextId(0)
        , missionId(0)
        , timeStepIntervalMillisec(-1)
        , lastRecordedSession(-1)
        , sourceType(EPersistenceSourceType::UNDEFINED)
    {}

    static constexpr TPersistenceSetId INVALID_PERSISTENCE_ID = -1;

    TPersistenceSetId persistenceSetId;
    TContextId contextId;
    TMissionId missionId;
    int64_t timeStepIntervalMillisec;
    TSessionNum lastRecordedSession;
    EPersistenceSourceType sourceType;
};

struct SPersistenceMetadataVideo : SPersistenceMetadataDescr {
    TSensorId recordedFromSensorId;
};

struct SPersistenceMetadataDSS : SPersistenceMetadataDescr {
    bool realData;
    EPersistenceDataType dataType;
};

struct SPersistenceMetadataRaw : SPersistenceMetadataDescr {

};


struct SPersistenceMetadata {
    std::vector<SPersistenceMetadataVideo> persistenceFromVideo;
    std::vector<SPersistenceMetadataDSS> persistenceFromDSS;
    std::vector<SPersistenceMetadataRaw> persistenceFromRaw;
};

struct SPersistenceObj {
    enum class EState : int32_t {
        ACTIVE,
        DESTROYED,
        ABSENT,
        UNDEFINED
    };

    TContextId ctxId;
    TMissionId missionId;
    TObjectId objId;
    EState state;
    TSessionNum sessionNum;
    TLogicStep logicTime;
    int64_t astroTimeMillisec;
};

struct SPersistenceTrajectory : SPersistenceObj {

    float latDeg;
    float lonDeg;
    double yawDeg;
};

struct SPersistenceWeather : SPersistenceObj {

    double windSpeed;
    double humidity;
};

struct SPersistenceSetFilter {
    SPersistenceSetFilter()
        : persistenceSetId(-1)
        , sessionNum(0)
        , maxLogicStep(0)
        , minLogicStep(0)
    {}

    TPersistenceSetId persistenceSetId;

    TSessionNum sessionNum;
    TLogicStep maxLogicStep;
    TLogicStep minLogicStep;
};

struct SObjectStep {
    TLogicStep logicStep;
    int64_t timestampMillisec;
};

struct SEventsSessionInfo {
    SEventsSessionInfo(){
        clear();
    }

    void clear(){
        number = 0;
        minLogicStep = 0;
        maxLogicStep = 0;
        minTimestampMillisec = 0;
        maxTimestampMillisec = 0;
        steps.clear();
    }

    bool empty(){
        return (
        number == 0 &&
        minLogicStep == 0 &&
        maxLogicStep == 0 &&
        minTimestampMillisec == 0 &&
        maxTimestampMillisec == 0 &&
        steps.empty() );
    }

    // for sort
    bool operator<( const SEventsSessionInfo & _rhs ){
        return ( this->number < _rhs.number );
    }

    TSessionNum number;
    TLogicStep minLogicStep;
    TLogicStep maxLogicStep;
    int64_t minTimestampMillisec;
    int64_t maxTimestampMillisec;
    std::vector<SObjectStep> steps;
};

struct SUserState {
    TUserId userId;
    std::string userIp;
    TPid userPid;
    int64_t lastPingMillisec;
};
using PUserState = std::shared_ptr<SUserState>;

struct FunctorObjectStep {
    FunctorObjectStep( TLogicStep _stepToFind )
        : stepToFind(_stepToFind)
    {}

    bool operator()( const SObjectStep & _rhs ){
        return ( stepToFind == _rhs.logicStep );
    }

    TLogicStep stepToFind;
};

// ---------------------------------------------------------------------------
// exchange ADT ( component <-> store, component <-> network, etc... )
// ---------------------------------------------------------------------------
// WAL records
struct SWALRecord {


    virtual ~SWALRecord(){}

    virtual void accept( IWALRecordVisitor * _visitor ) const = 0;
    virtual std::string serializeToStr() const { return "something_shit"; }
};

struct SWALClientOperation : SWALRecord {

    using TUniqueKey = std::string;

    static const TUniqueKey ALL_KEYS;
    static const TUniqueKey NON_INTEGRITY_KEYS;

    virtual void accept( IWALRecordVisitor * _visitor ) const override;
    virtual std::string serializeToStr() const override;

    TUniqueKey uniqueKey;
    bool begin;
    std::string commandFullText;
};

struct SWALUserRegistration : SWALRecord {

    using TRegisterId = std::string;

    static const TRegisterId ALL_IDS;

    virtual void accept( IWALRecordVisitor * _visitor ) const override;
    virtual std::string serializeToStr() const override;

    TRegisterId registerId;
    std::string userIp;
    TPid userPid;
    std::string registeredAtDateTime;
};

struct SWALProcessEvent : SWALRecord {

    using TUniqueKey = TPid;

    static constexpr TPid ALL_PIDS = 0;
    static constexpr TPid NON_INTEGRITY_PIDS = 1;

    virtual void accept( IWALRecordVisitor * _visitor ) const override;
    virtual std::string serializeToStr() const override;

    TUniqueKey pid;
    bool begin;
    std::string programName;
    std::vector<std::string> programArgs;
};

struct SWALOnceMoreRecord : SWALRecord {

    virtual void accept( IWALRecordVisitor * _visitor ) const override;

};

// persistence
struct SPersistenceEntity {
    virtual ~SPersistenceEntity(){}

    virtual void accept( IPersistenceEntityVisitor * _visitor ) const = 0;

    TPersistenceSetId persistenceSetId;
};

struct SPersistenceTrajectorySet : SPersistenceEntity {

    virtual void accept( IPersistenceEntityVisitor * _visitor ) const override;

    std::vector<SPersistenceTrajectory> trajectories;
};

struct SPersistenceWeatherSet : SPersistenceEntity {

    virtual void accept( IPersistenceEntityVisitor * _visitor ) const override;

    std::vector<SPersistenceWeather> weathers;
};


// ---------------------------------------------------------------------------
// types deduction
// ---------------------------------------------------------------------------
class IWALRecordVisitor {
public:
    virtual ~IWALRecordVisitor(){}

    virtual void visit( const SWALClientOperation * _record ) = 0;
    virtual void visit( const SWALProcessEvent * _record ) = 0;
    virtual void visit( const SWALUserRegistration * _record ) = 0;
    virtual void visit( const SWALOnceMoreRecord * _record ) = 0;

};

class IPersistenceEntityVisitor {
public:
    virtual ~IPersistenceEntityVisitor(){}

    virtual void visit( const SPersistenceTrajectorySet * _record ) = 0;
    virtual void visit( const SPersistenceWeatherSet * _record ) = 0;
};


// ---------------------------------------------------------------------------
// service interfaces
// ---------------------------------------------------------------------------
class IContextService {
public:
    virtual ~IContextService(){}

    virtual bool openContext( TContextId _ctxId ) = 0;
    virtual bool openContextAsync( TContextId _ctxId ) = 0;
    virtual bool closeContext() = 0;

    virtual common_types::TContextId getCurrentContextId() = 0;
    virtual common_types::TContextId getContextIdByName( const std::string & _ctxName ) = 0;
    virtual std::string getContextNameById( common_types::TContextId _ctxId ) = 0;
};

class IWALPersistenceService {
public:
    virtual ~IWALPersistenceService(){}

    virtual bool write( const SWALClientOperation & _clientOperation ) = 0;
    virtual void remove( SWALClientOperation::TUniqueKey _filter ) = 0;
    virtual const std::vector<SWALClientOperation> readOperations( SWALClientOperation::TUniqueKey _filter ) = 0;

    virtual bool write( const SWALProcessEvent & _processEvent ) = 0;
    virtual void remove( SWALProcessEvent::TUniqueKey _filter ) = 0;
    virtual const std::vector<SWALProcessEvent> readEvents( SWALProcessEvent::TUniqueKey _filter ) = 0;

    virtual bool write( const SWALUserRegistration & _userRegistration ) = 0;
    virtual void removeRegistration( SWALUserRegistration::TRegisterId _filter ) = 0;
    virtual const std::vector<SWALUserRegistration> readRegistrations( SWALUserRegistration::TRegisterId _filter ) = 0;
};



// ---------------------------------------------------------------------------
// service locator
// ---------------------------------------------------------------------------
struct SIncomingCommandGlobalServices {


};


}

#endif // MS_COMMON_TYPES_H

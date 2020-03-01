#ifndef MS_COMMON_TYPES_H
#define MS_COMMON_TYPES_H

#include <memory>
#include <string>

#include "communication/network_interface.h"


namespace common_types {


// ---------------------------------------------------------------------------
// forwards
// ---------------------------------------------------------------------------
class IWALRecordVisitor;



// ---------------------------------------------------------------------------
// global typedefs
// ---------------------------------------------------------------------------

// system types
using TPid = pid_t;
using TCommandLineArgKey = std::string;
using TCommandLineArgVal = std::string;

// objrepr types
using TContextId = uint32_t;
using TMissionId = uint32_t;
using TSensorId = uint64_t;
using TObjectId = uint64_t;

// player types
using TTimeRange = std::pair<int64_t, int64_t>;
using TSession = int32_t;
using TLogicStep = int64_t;


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


// ---------------------------------------------------------------------------
// simple ADT
// ---------------------------------------------------------------------------





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




// ---------------------------------------------------------------------------
// types deduction
// ---------------------------------------------------------------------------





// ---------------------------------------------------------------------------
// service interfaces
// ---------------------------------------------------------------------------
class ICommunicationService {
public:
    virtual ~ICommunicationService(){}

    virtual PNetworkEntity getConnection( INetworkEntity::TConnectionId _connId ) = 0;
    virtual PNetworkClient getFileDownloader() = 0;
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
};



// ---------------------------------------------------------------------------
// service locator
// ---------------------------------------------------------------------------
struct SIncomingCommandGlobalServices {


};


}

#endif // MS_COMMON_TYPES_H

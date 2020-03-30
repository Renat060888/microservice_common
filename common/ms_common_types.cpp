
#include "ms_common_types.h"

using namespace std;
using namespace common_types;

const SWALClientOperation::TUniqueKey SWALClientOperation::ALL_KEYS = "all_keys";
const SWALClientOperation::TUniqueKey SWALClientOperation::NON_INTEGRITY_KEYS = "non_integrity_keys";

// wal
void SWALClientOperation::accept( IWALRecordVisitor * _visitor ) const {
    _visitor->visit( this );
}

void SWALProcessEvent::accept( IWALRecordVisitor * _visitor ) const {
    _visitor->visit( this );
}

void SWALOnceMoreRecord::accept( IWALRecordVisitor * _visitor ) const {
    _visitor->visit( this );
}

void SWALUserRegistration::accept( IWALRecordVisitor * _visitor ) const {
    _visitor->visit( this );
}

string SWALClientOperation::serializeToStr() const {
    return string();
}

string SWALUserRegistration::serializeToStr() const {
    return string();
}

string SWALProcessEvent::serializeToStr() const {
    return string();
}

#ifndef MS_COMMON_UTILS_H
#define MS_COMMON_UTILS_H

#include <cassert>
#include <bitset>
#include <signal.h>
#include <sys/file.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string.h>
#include <thread>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/filesystem.hpp>

#include "ms_common_types.h"

namespace common_utils {

// способ вывода для компонентов системы в момент, когда основной логгер еще не инициализирован
#define PRELOG_INFO std::cout
#define PRELOG_ERR std::cerr

// --------------------------------------------------------------
// convertors
// --------------------------------------------------------------
inline std::string convertPersistenceObjStateToStr( common_types::SPersistenceObj::EState _state ){
    switch (_state) {
    case common_types::SPersistenceObj::EState::ABSENT : return "ABSENT";
    case common_types::SPersistenceObj::EState::ACTIVE : return "ACTIVE";
    case common_types::SPersistenceObj::EState::DESTROYED : return "DESTROYED";
    case common_types::SPersistenceObj::EState::UNDEFINED : return "UNDEFINED";
    default : assert( false && "unknown persistence object state" );
    }
}

inline std::string convertPersistenceTypeToStr( common_types::EPersistenceSourceType _type ){
    switch (_type) {
    case common_types::EPersistenceSourceType::VIDEO_SERVER : return "VIDEO_SERVER";
    case common_types::EPersistenceSourceType::AUTONOMOUS_RECORDER : return "AUTONOMOUS_RECORDER";
    case common_types::EPersistenceSourceType::DSS : return "DSS";
    case common_types::EPersistenceSourceType::UNDEFINED : return "UNDEFINED";
    default : assert( false && "unknown persistence source type enum" );
    }
}

inline common_types::EPersistenceSourceType convertPersistenceTypeFromStr( const std::string & _type ){

    if( "VIDEO_SERVER" == _type ){ return common_types::EPersistenceSourceType::VIDEO_SERVER; }
    else if( "AUTONOMOUS_RECORDER" == _type ){ return common_types::EPersistenceSourceType::AUTONOMOUS_RECORDER; }
    else if( "DSS" == _type ){ return common_types::EPersistenceSourceType::DSS; }
    else if( "UNDEFINED" == _type ){ return common_types::EPersistenceSourceType::UNDEFINED; }
    else{ assert( false && "unknown persistence source type str: " && _type.c_str() ); }
}

// --------------------------------------------------------------
// language
// --------------------------------------------------------------


// --------------------------------------------------------------
// network
// --------------------------------------------------------------
inline std::string getIpAddressStr(){

    ifaddrs * interfaces = nullptr;
    std::string ip;

    getifaddrs( & interfaces );

    for( ifaddrs * itf = interfaces; itf != nullptr; itf = itf->ifa_next ){
        if( ! itf->ifa_addr ){
            continue;
        }

        if( AF_INET == itf->ifa_addr->sa_family ){
            in_addr address = ( (sockaddr_in *)itf->ifa_addr )->sin_addr;
            char * str = inet_ntoa( address );
            ip.assign( str, strlen(str) );

            if( std::string("127.0.0.1") != ip ){
                freeifaddrs( interfaces );
                return ip;
            }
        }
    }

    freeifaddrs( interfaces );
    return ip;
}

inline uint32_t ipAddressToOctets( const std::string & _ip ){

    uint8_t octets[ 4 ];
    memset( octets, 0, sizeof(octets) );
    std::string::size_type strIdxTo = 0;
    std::string::size_type strIdxFrom = 0;

    for( int octetIdx = 0; octetIdx < 4; octetIdx++ ){

        strIdxTo = _ip.find_first_of( ".", strIdxFrom );

        if( strIdxTo != std::string::npos ){
            const std::string octetStr = _ip.substr( strIdxFrom, strIdxTo - strIdxFrom );
            octets[ octetIdx ] = std::stoi( octetStr );
            strIdxFrom = strIdxTo + 1;
        }
        else{
            const std::string octetStr = _ip.substr( strIdxFrom, _ip.size() - strIdxFrom );
            octets[ octetIdx ] = std::stoi( octetStr );
        }
    }

    uint32_t out = 0;
    out |= octets[ 0 ] << 24;
    out |= octets[ 1 ] << 16;
    out |= octets[ 2 ] << 8;
    out |= octets[ 3 ];

    const std::string check = std::bitset<32>( out ).to_string();
    return out;
}

inline std::string ipAddressFromOctets( uint32_t _octets ){

    uint8_t octets[ 4 ];
    memset( octets, 0, sizeof(octets) );

    octets[ 0 ] |= _octets >> 24;
    octets[ 1 ] |= _octets >> 16;
    octets[ 2 ] |= _octets >> 8;
    octets[ 3 ] |= _octets;

    std::string ip;

    for( int octetIdx = 0; octetIdx < 4; octetIdx++ ){

        const std::string octetStr = std::to_string( octets[ octetIdx ] );
        ip.append( octetStr );

        if( octetIdx < 3 ){
            ip.append( "." );
        }
    }

    return ip;
}

// --------------------------------------------------------------
// system
// --------------------------------------------------------------
using TSignalHandlerFunction = void( * )( int );

inline void connectInterruptSignalHandler( TSignalHandlerFunction _funcPtr ){

    struct sigaction sigHandler;
    sigHandler.sa_handler = _funcPtr;
    sigHandler.sa_flags = 0;
    sigemptyset( & sigHandler.sa_mask );

    // 'subscribe' to SIGINT
    sigaction( SIGINT, & sigHandler, nullptr );
}

inline bool threadShutdown( std::thread * & _thread ){

    if( _thread ){
        if( _thread->joinable() ){
            _thread->join();
            delete _thread;
            _thread = nullptr;
            return true;
        }
        else{
            return false;
        }
    }
    else{
        return false;
    }
}

inline std::string generateUniqueId(){

    const boost::uuids::uuid uniqueId = boost::uuids::random_generator()();

    std::ostringstream oss;
    oss << uniqueId;

    return oss.str();

#if 0
    uuid_t id;
    uuid_generate( id );

    char uuid_str[37];
    uuid_unparse_lower( id, uuid_str );

    return std::string( uuid_str );
#endif
}

inline void generateUniqueId( char * _array ){

    const boost::uuids::uuid uniqueId = boost::uuids::random_generator()();

    std::ostringstream oss;
    oss << uniqueId;

    const std::string str = oss.str();

    // copy uuid '8-4-4-4-12' fortam string to array
    ::memcpy( _array, str.data(), 36 );
}

// --------------------------------------------------------------
// string
// --------------------------------------------------------------
inline std::pair<std::string, std::string> cutBySimbol( char _simb, const std::string & _str ){

    std::pair<std::string, std::string> out;

    if( _str.find(_simb) == std::string::npos ){
        out.second = _str;
    }
    else{
        std::string::size_type pos = _str.find(_simb);
        out.first = _str.substr( 0, pos );
        out.second = _str.substr( pos + 1, _str.size() - pos );
    }

    return out;
}

inline std::string filterByDigitAndNumber( std::string _str ){

    for( char & sym : _str ){
        if( ! ((sym >= '0' && sym <= '9') || (sym >= 'a' && sym <= 'z') || (sym >= 'A' && sym <= 'Z' )) ){
            sym = '_';
        }
    }

    return _str;
}

inline std::string getLastLine( const std::string _filePath ){

    std::ifstream originalPlaylistFile;

    originalPlaylistFile.open( _filePath, std::ios::in );
    if( ! originalPlaylistFile.is_open() ){
        PRELOG_ERR << "ERROR: can't open file [" << _filePath << "] for reading" << std::endl;
        std::string();
    }

    // find ending line
    originalPlaylistFile.seekg( -1, std::ios_base::end );

    bool keepLooping = true;
    while( keepLooping ){
        char ch;
        originalPlaylistFile.get( ch );
        if( (int)originalPlaylistFile.tellg() <= 1 ){
            originalPlaylistFile.seekg( 0 );
            keepLooping = false;
        }
        else if( '\n' == ch ){
            keepLooping = false;
        }
        else{
            originalPlaylistFile.seekg( -2, std::ios_base::cur );
        }
    }

    std::string lastLine;
    std::getline( originalPlaylistFile, lastLine );
    originalPlaylistFile.close();

    return lastLine;
}

inline std::string wstrToStr(const std::wstring & _wstr){
    char buffer[ _wstr.size() * 2 ];
    const int rt = ::wcstombs( buffer, _wstr.data(), sizeof(buffer) );
    if( ! rt ){
        PRELOG_ERR << "failed to convert a wide char string to multibyte string" << std::endl;
    }
    return buffer;
}

inline std::wstring strToWstr(const std::string _str){
    return std::wstring( _str.begin(), _str.end() );
}

// --------------------------------------------------------------
// filesystem
// --------------------------------------------------------------
inline bool isDirectory( const std::string & _fsEntity ){
    namespace fs = boost::filesystem;

    try{
        fs::path path( _fsEntity );
        if( fs::exists(path) && fs::is_directory(path) ){
            return true;
        }
    }
    catch( fs::filesystem_error & _ex ){
        PRELOG_ERR << _ex.what() << std::endl;
    }
    return false;
}

// --------------------------------------------------------------
// time
// --------------------------------------------------------------
inline int64_t timeMillisecToHour( int64_t _millisec ){
    return ( _millisec / 1000 / 60 / 60 );
}

inline int64_t timeHourToMillisec( int64_t _hour ){
    return ( _hour * 60 * 60 * 1000 );
}

// TODO: time zone UTC +3
#define TIME_ZONE_MILLISEC 1000 * 60 * 60 * 3 // sec * min * hour * 3

inline int64_t getCurrentTimeMillisec(){

    using namespace std::chrono;

    auto currentTime = high_resolution_clock::now().time_since_epoch();

    return duration_cast<milliseconds>(currentTime).count() + TIME_ZONE_MILLISEC;
}

inline int64_t getCurrentTimeMillisec2(){
    using namespace std::chrono;

    auto currentTime = high_resolution_clock::now().time_since_epoch();
    return duration_cast<milliseconds>(currentTime).count();
}

// TODO: time zone UTC +3
#define TIME_ZONE_SEC 60 * 60 * 3 // sec * min * hour * 3

inline int64_t getCurrentTimeSec(){
    using namespace std::chrono;

    auto currentTime = high_resolution_clock::now().time_since_epoch();
    return duration_cast<seconds>(currentTime).count() + TIME_ZONE_SEC;
}

inline int64_t getCurrentTimeSec2(){
    using namespace std::chrono;

    auto currentTime = high_resolution_clock::now().time_since_epoch();
    return duration_cast<seconds>(currentTime).count();
}

// TODO: time zone UTC +3
#define TIME_ZONE_MIN 60 * 3 // min * hour * 3

inline int64_t getCurrentTimeMinutes(){

    using namespace std::chrono;

    auto currentTime = high_resolution_clock::now().time_since_epoch();

    return duration_cast<minutes>(currentTime).count() + TIME_ZONE_MIN;
}

inline int64_t timeStrToMillisec( const std::string & _timeStr /*char _dateTimeDelimeter*/ ){

    tm timeTM;
    memset( & timeTM, 0, sizeof(tm) );
    setenv("TZ", "RUS0", 1);
    tzset();
    strptime( _timeStr.c_str(), "%F %T", & timeTM );
    time_t timeTIME_T = mktime( & timeTM );

    return timeTIME_T * 1000;
}

inline std::string timeMillisecToStr( int64_t _timeMillisec /*char _dateTimeDelimeter*/ ){

    time_t timeSec = _timeMillisec / 1000;
    char timeBuf[ 1024 ];
    setenv("TZ", "RUS0", 1);
    tzset();
    strftime( timeBuf, sizeof(timeBuf), "%F %T", gmtime( & timeSec ) );
    return std::string( timeBuf );
}

inline std::string getCurrentDateTimeStr(){

    return timeMillisecToStr( getCurrentTimeMillisec() );
}

// --------------------------------------------------------------
// ...
// --------------------------------------------------------------



}

#endif // MS_COMMON_UTILS_H



#include <cassert>
#include <iomanip>
#include <unordered_map>
#include <regex>
#include <mutex>
#include <sys/sysinfo.h>
#include <fstream>
#include <unistd.h>

#include <glibtop.h>
#include <glibtop/cpu.h>
#include <nvml.h>

#include "logger.h"
#include "system_monitor.h"

using namespace std;

static constexpr const char * PRINT_HEADER = "SystemMonitor:";
static const std::regex REGEX_FOR_NUMBER( R"((?:^|\s)([+-]?[[:digit:]]+(?:\.[[:digit:]]+)?)(?=$|\s))" );

// -------------------------------------------------------------------------
// utils
// -------------------------------------------------------------------------
static int getIntFromStr( const string & _str ){

    int out = -1;

    std::smatch m;
    std::string str = _str;
    if( std::regex_search( str, m, REGEX_FOR_NUMBER ) ){
        out = stoi( m[ 1 ] );
    }

    return out;
}

static vector<int> getIntsFromStr( const string & _str ){

    vector<int> out;

    std::smatch m;
    std::string str = _str;
    while( std::regex_search( str, m, REGEX_FOR_NUMBER ) ){
        out.push_back( stoi(m[ 1 ]) );
        str = m.suffix().str(); // proceed next match
    }

    return out;
}

static float getFloatFromStr( const string & _str ){

    float out = -1.0f;

    std::smatch m;
    std::string str = _str;
    if( std::regex_search( str, m, REGEX_FOR_NUMBER ) ){
        out = stof( m[ 1 ] );
    }

    return out;
}

static string getStringAfterSemicolon( const string & _str ){

    string out;

    //
    string::size_type posSemicolon = _str.find_first_of(":");
    string::size_type stringBeginPos;
    for( stringBeginPos = posSemicolon + 1; stringBeginPos < _str.size(); stringBeginPos++ ){
        if( _str[ stringBeginPos ] == ' ' ){
            continue;
        }
        else{
            break;
        }
    }

    //
    out = _str.substr( stringBeginPos, _str.size() - stringBeginPos );

    return out;
}

// -------------------------------------------------------------------------
// data types
// -------------------------------------------------------------------------
struct SPrivateDataSection {
    unordered_map<int, nvmlDevice_t> cardsNvidiaDescriptions;
    unordered_map<int, SystemMonitor::SVideoStatus> cardsStatuses;
    std::mutex statusesLock;
    std::string lastError;
};

// -------------------------------------------------------------------------
// class
// -------------------------------------------------------------------------
SystemMonitor::SystemMonitor()
{
    m_impl = new SPrivateDataSection();

    // Initialize the library.
    nvmlReturn_t result = nvmlInit();
    if( NVML_SUCCESS != result ){
        VS_LOG_ERROR << PRINT_HEADER << " failed to initialize [" << nvmlErrorString( result ) << "]" << endl;
    }

    // Get the number of installed devices.
    unsigned int device_count = 0;
    result = nvmlDeviceGetCount( & device_count );
    if( NVML_SUCCESS != result ){
        VS_LOG_ERROR << PRINT_HEADER << " failed to get device count [" << nvmlErrorString( result ) << "]" << endl;
    }
    VS_LOG_INFO << PRINT_HEADER << " found video devices [" << device_count << "]" << endl;

    // Iterate through the devices.
    for( unsigned int device_index = 0; device_index < device_count; device_index++ ){

        // Get the device's handle.
        nvmlDevice_t device;
        result = nvmlDeviceGetHandleByIndex( device_index, & device );
        if( NVML_SUCCESS == result ){
            m_impl->cardsNvidiaDescriptions.insert( {device_index, device} );
        }
        else{
            VS_LOG_ERROR << PRINT_HEADER << " failed to get handle for device [" << nvmlErrorString( result ) << "]" << endl;
        }
    }

    // init availability container
    const vector<SVideoStatus> videoStats = getVideos();
    for( const SVideoStatus & videocard : videoStats ){
        m_impl->cardsStatuses.insert( {videocard.index, videocard} );
    }
}

SystemMonitor::~SystemMonitor()
{
    delete m_impl;

    // Shutdown the library.
    nvmlReturn_t result = nvmlShutdown();
    if( NVML_SUCCESS != result ){
        VS_LOG_ERROR << PRINT_HEADER << " failed to shutdown [" << nvmlErrorString( result ) << "]" << endl;
    }

}

const std::string & SystemMonitor::getLastError(){
    return m_impl->lastError;
}

SystemMonitor::STotalInfo SystemMonitor::getTotalSnapshot(){

    SystemMonitor::STotalInfo info;

    info.videocards = getVideos();
    info.cpu = getCPUStatus();
    info.memory = getMemoryStatus();
    info.hostname = getHostnameAndUser().first;
    info.user = getHostnameAndUser().second;
    info.uptimeSec = getUptimeSec();

    return info;
}

static guint64 g_lastTotalCpu = 0;
static guint64 g_lastUsedCpu = 0;

long SystemMonitor::getUptimeSec(){

    struct sysinfo info;
    ::sysinfo( & info );

    return info.uptime;
}


std::pair<std::string,std::string> SystemMonitor::getHostnameAndUser(){

    std::pair<std::string,std::string> p;

    //
    constexpr int hostnameBufLen = 1024;
    char hostnameBuf[ hostnameBufLen ];
    ::gethostname( hostnameBuf, hostnameBufLen );

    p.first = hostnameBuf;

    //
    p.second = "ble";





    return p;
}

SystemMonitor::SCPUStatus SystemMonitor::getCPUStatus(){

    SystemMonitor::SCPUStatus out;

    //
    ifstream cpuFile( "/proc/cpuinfo", std::ios::in );
    if( ! cpuFile.is_open() ){
        VS_LOG_ERROR << "cannot open '/proc/cpuinfo'" << endl;
        return out;
    }

//    out.totalLoadPercent = getTotalLoad( statStream );
    const std::vector<std::string> coreStatLines = getCoreStatLines();

    int idx = 0;
    string line;
    stringstream oneCoreInfo;
    while( getline(cpuFile, line) ){

        if( line.empty() ){
            SCPUStatus::SCoretatus core = getCoreStatus( oneCoreInfo );
            oneCoreInfo.clear();

            core.index = idx++;

            core.loadPercent = getCoreLoad( coreStatLines[ core.index ] );

            out.cores.push_back( core );

            // NOTE: core names equal CPU name
            out.name = core.name;
        }
        else{
            oneCoreInfo << line << endl;
        }
    }

    float totalLoad = 0;
    for( int i = 0; i < out.cores.size(); i++ ){
        totalLoad += out.cores[ i ].loadPercent;
    }

    glibtop_cpu cpu;
    glibtop_get_cpu( & cpu );

    const float total = cpu.total - g_lastTotalCpu;
    const float used = (cpu.user + cpu.nice + cpu.sys) - g_lastUsedCpu;
    const float load = used / std::max( total, 1.0f );

    g_lastTotalCpu = total;
    g_lastUsedCpu = used;

//    out.totalLoadByAvgFromCoresPercent = totalLoad / out.cores.size();
    out.totalLoadByAvgFromCoresPercent = load * 100.0f;

    return out;
}

// TODO: trash
// -------------------------------------------------------------------------------
#include <numeric>

static vector<size_t> get_cpu_times(){
    std::ifstream proc_stat("/proc/stat");
    proc_stat.ignore( 5, ' ' );
    vector<size_t> times;
    for( size_t time; proc_stat >> time; times.push_back(time) );
    return times;
}

static bool get_cpu_times( size_t & _idle_time, size_t & _total_time ){
    const vector<size_t> cpu_times = get_cpu_times();
    if( cpu_times.size() < 4 ){
        return false;
    }
    _idle_time = cpu_times[ 3 ];
    _total_time = std::accumulate( cpu_times.begin(), cpu_times.end(), 0 );
    return true;
}

static bool get_cpu_times2( size_t & _work_time, size_t & _total_time ){
    const vector<size_t> cpu_times = get_cpu_times();
    if( cpu_times.size() < 4 ){
        assert( false && "cpu times size => 4" );
    }
    _work_time = cpu_times[ 0 ] + cpu_times[ 1 ] + cpu_times[ 2 ];
    _total_time = std::accumulate( cpu_times.begin(), cpu_times.end(), 0 );
    return true;
}
// -------------------------------------------------------------------------------

float SystemMonitor::getCoreLoad( const string & _coreStatLine ){

    const string::size_type posSpace = _coreStatLine.find_first_of(" ");
    const string numbers = _coreStatLine.substr( posSpace + 1, _coreStatLine.size() - posSpace );
    vector<int> coreTimes = getIntsFromStr( numbers );

    const float work_time = coreTimes[ 0 ] + coreTimes[ 1 ] + coreTimes[ 2 ];
    const float total_time = std::accumulate( coreTimes.begin(), coreTimes.end(), 0 );

    const float utilization = ( work_time / total_time ) * 100.0;

    return utilization;
}

float SystemMonitor::getTotalLoad( std::stringstream & _ss ){

    // TODO: temp solution 2
    size_t work_time_p = 0, total_time2_p = 0;
    size_t work_time = 0, total_time2 = 0;
    get_cpu_times2( work_time, total_time2 );
    const float utilization2 = ( (float)(work_time-work_time_p) / (float)(total_time2-total_time2_p) ) * 100.0;
    work_time_p = work_time;
    total_time2 = total_time2_p;
    VS_LOG_INFO << "CPU util [" << utilization2 << "]" << endl;
    return utilization2;

    // TODO: temp solution
    size_t previous_idle_time = 0, previous_total_time = 0;
    size_t idle_time = 0, total_time = 0;
    get_cpu_times( idle_time, total_time );
    const float idle_time_delta = idle_time - previous_idle_time;
    const float total_time_delta = total_time - previous_total_time;
    const float utilization = 100.0 * ( 1.0 - idle_time_delta / total_time_delta );
    return utilization;



    // TODO: do
    int out = 777;
    string line;
    while( getline(_ss, line) ){

    }
    return out;
}

std::vector<std::string> SystemMonitor::getCoreStatLines(){

    std::vector<std::string> out;

    ifstream statFile("/proc/stat");
    if( ! statFile.is_open() ){
        VS_LOG_ERROR << "cannot open '/proc/stat'" << endl;
        return out;
    }

    const int rt = get_nprocs();

    int counter = 0;
    string line;
    while( getline(statFile, line) ){

        if( line.find( string("cpu") + std::to_string(counter) ) != string::npos ){
            counter++;
            out.push_back( line );
        }

        if( counter >= rt ){
            break;
        }
    }

    return out;
}

SystemMonitor::SCPUStatus::SCoretatus SystemMonitor::getCoreStatus( std::stringstream & _ss ){

    SystemMonitor::SCPUStatus::SCoretatus out;

    string line;
    while( getline(_ss, line) ){

        if( line.find("model name") != string::npos ){
            out.name = getStringAfterSemicolon( line );
        }
        else if( line.find("cpu MHz") != string::npos ){
            out.frequencyMHz = getFloatFromStr( line );
        }
    }

    return out;
}

SystemMonitor::SMemoryStatus SystemMonitor::getMemoryStatus(){

    SystemMonitor::SMemoryStatus out;

    ifstream memFile("/proc/meminfo");
    if( ! memFile.is_open() ){
        VS_LOG_ERROR << "cannot open '/proc/meminfo'" << endl;
        return out;
    }

    string line;
    while( getline(memFile, line) ){

        if( line.find("MemTotal") != string::npos ){
            out.ramTotalMb = getIntFromStr( line ) / 1024;
        }
        else if( line.find("MemFree") != string::npos ){
            out.ramFreeMb = getIntFromStr( line ) / 1024;
        }
        else if( line.find("MemAvailable") != string::npos ){
            out.ramAvailableMb = getIntFromStr( line ) / 1024;
        }
        else if( line.find("SwapCached") != string::npos ){
            out.swapCached = getIntFromStr( line ) / 1024;
        }
    }

    return out;
}

vector<SystemMonitor::SVideoStatus> SystemMonitor::getVideos(){

    /**
     * Iterate through all the devices on the system which are recognised by NVML.
     * For each device, print out:
     * - The device's index.
     * - The device's name.
     * - The device's PCI info.
     * - The device's utilisation, if supported.
     * - The device's ECC mode, if supported.
     * - The device's fan speed, if supported.
     * - The device's temperature.
     * - The device's persistence mode.
     * - The device's power usage, if supported.
     */

    vector<SystemMonitor::SVideoStatus> out;

    for( auto iter = m_impl->cardsNvidiaDescriptions.begin(); iter != m_impl->cardsNvidiaDescriptions.end(); ++iter ){
        nvmlDevice_t & device = iter->second;

        SystemMonitor::SVideoStatus videoStatusOut;

        // Get the device's name.
        char device_name[ NVML_DEVICE_NAME_BUFFER_SIZE ];
        nvmlReturn_t result = nvmlDeviceGetName( device, device_name, NVML_DEVICE_NAME_BUFFER_SIZE );
        if( NVML_SUCCESS == result ){
            videoStatusOut.cardName = device_name;
        }
        else{
            VS_LOG_ERROR << PRINT_HEADER << " failed to get device name [" << nvmlErrorString( result ) << "]" << endl;
        }

        nvmlPciInfo_t pci_info;
        result = nvmlDeviceGetPciInfo( device, & pci_info );
        if( NVML_SUCCESS == result ){

            unsigned int device_id, vendor_id;
            unsigned int subsystem_device_id, subsystem_vendor_id;
            device_id = pci_info.pciDeviceId >> 16;
            vendor_id = pci_info.pciDeviceId & 0xffff;
            subsystem_device_id = pci_info.pciSubSystemId >> 16;
            subsystem_vendor_id = pci_info.pciSubSystemId & 0xffff;
        }
        else{
            VS_LOG_ERROR << PRINT_HEADER << " failed to get PCI info [" << nvmlErrorString( result ) << "]" << endl;
        }

        // Get the device's utilization.
        nvmlUtilization_t utilization;
        result = nvmlDeviceGetUtilizationRates( device, & utilization );
        if( NVML_SUCCESS == result ){
            videoStatusOut.gpuLoadPercent = utilization.gpu;
            videoStatusOut.memoryUsedPercent = utilization.memory;
        }
        else{
            VS_LOG_WARN << PRINT_HEADER << " failed to get device utilization [" << nvmlErrorString( result ) << "]" << endl;
        }

        // Get the current and pending ECC modes.
        nvmlEnableState_t current, pending;
        result = nvmlDeviceGetEccMode( device, & current, & pending );
        if( NVML_SUCCESS == result ){
            if( NVML_FEATURE_ENABLED == current ){
                int a = 0;
            }
            else {
                int a = 0;
            }
        }
        else{
//            VS_LOG_ERROR << PRINT_HEADER << " failed to get ECC mode [" << nvmlErrorString( result ) << "]" << endl;
        }

        // Get the device's fan speed.
        unsigned int fan_speed = 0;
        result = nvmlDeviceGetFanSpeed( device, & fan_speed );
        if( NVML_SUCCESS == result ){
            int a = 0;
        }
        else{
            VS_LOG_ERROR << PRINT_HEADER << " failed to get device fan speed [" << nvmlErrorString( result ) << "]" << endl;
        }

        // Get the device's temperature.
        unsigned int temp = 0;
        result = nvmlDeviceGetTemperature( device, NVML_TEMPERATURE_GPU, & temp );
        if( NVML_SUCCESS == result ){
            int a = 0;
        }
        else{
            VS_LOG_ERROR << PRINT_HEADER << " failed to get device temperature [" << nvmlErrorString( result ) << "]" << endl;
        }

        // Get the device's power usage.
        unsigned int power_usage = 0;
        result = nvmlDeviceGetPowerUsage( device, & power_usage );
        if( NVML_SUCCESS == result ){
            int a = 0;
        }
        else{            
            VS_LOG_WARN << PRINT_HEADER << " failed to get device power usage [" << nvmlErrorString( result ) << "]" << endl;
        }

        // Get the device's persistence mode.
        nvmlEnableState_t persistence_mode;
        result = nvmlDeviceGetPersistenceMode( device, & persistence_mode );
        if( NVML_SUCCESS == result ){
            if( NVML_FEATURE_ENABLED == persistence_mode ){
                int a = 0;
            }
            else{
                int a = 0;
            }
        }
        else {
            VS_LOG_ERROR << PRINT_HEADER << " failed to get persistence mode [" << nvmlErrorString( result ) << "]" << endl;
        }

        //
        unsigned int nvmlIndex = 0;
        result = nvmlDeviceGetIndex( device, & nvmlIndex );
        videoStatusOut.index = nvmlIndex;

        //
        unsigned int infoCount = 0;
        result = nvmlDeviceGetComputeRunningProcesses( device, & infoCount, nullptr );
        nvmlProcessInfo_t infos[ infoCount ];
        result = nvmlDeviceGetComputeRunningProcesses( device, & infoCount, infos );
        if( NVML_SUCCESS == result ){
            for( unsigned int i = 0; i < infoCount; i++ ){
                int a = 0;
            }
        }
        else{            
            VS_LOG_WARN << PRINT_HEADER << " failed to get compute running processes for device [" << nvmlErrorString( result ) << "]" << endl;
        }

        out.push_back( videoStatusOut );
    }

    return out;
}

unsigned int SystemMonitor::getNextVideoCardIdx(){

    unsigned int idxWithMinLoad = NO_AVAILABLE_CARDS;
    unsigned int minGpuMemUtilization = std::numeric_limits<unsigned int>::max();
    // TODO: make selecting more sophisticated by Gpu core and it's Memory
    unsigned int minGpuUtilization = std::numeric_limits<unsigned int>::max();

    // search free card
    m_impl->statusesLock.lock();
    for( auto iter = m_impl->cardsStatuses.begin(); iter != m_impl->cardsStatuses.end(); ++iter ){
        SVideoStatus & cardStatus = iter->second;

        if( cardStatus.usedByVideoServer ){
            continue;
        }

        if( cardStatus.memoryUsedPercent < minGpuMemUtilization ){
            minGpuMemUtilization = cardStatus.memoryUsedPercent;
            idxWithMinLoad = cardStatus.index;
        }
    }
    m_impl->statusesLock.unlock();

    // make decision
    if( NO_AVAILABLE_CARDS == idxWithMinLoad ){
        stringstream ss;
        ss << PRINT_HEADER << " no free video card, try once more later";
        m_impl->lastError = ss.str();
        VS_LOG_ERROR << ss.str() << endl;
    }
    else{
        // check utilization
        const SVideoStatus & cardStatus = m_impl->cardsStatuses[ idxWithMinLoad ];
        constexpr unsigned int GPU_LOAD_PERCENT_LIMIT = 80;
        constexpr unsigned int GPU_MEMORY_LOAD_PERCENT_LIMIT = 60;
        if( cardStatus.memoryUsedPercent >= GPU_MEMORY_LOAD_PERCENT_LIMIT ){
            stringstream ss;
            ss << PRINT_HEADER
               << " memory utilization of most free card is more than [" << GPU_MEMORY_LOAD_PERCENT_LIMIT << "]"
               << " Select aborted";
            m_impl->lastError = ss.str();
            VS_LOG_ERROR << ss.str() << endl;

            return NO_AVAILABLE_CARDS;
        }

        //
        m_impl->statusesLock.lock();
        m_impl->cardsStatuses[ idxWithMinLoad ].usedByVideoServer = true;
        m_impl->statusesLock.unlock();

        VS_LOG_INFO << PRINT_HEADER
                 << " give most free card with idx [" << idxWithMinLoad << "]"
                 << " [" << cardStatus.cardName << "]"
                 << " gpu core load [" << cardStatus.gpuLoadPercent << "]%"
                 << " memory used [" << cardStatus.memoryUsedPercent << "]%"
                 << endl;
    }

    return idxWithMinLoad;
}

void SystemMonitor::releaseVideoCardIdx( unsigned int _idx ){

    std::lock_guard<std::mutex> lock( m_impl->statusesLock );

    auto iter = m_impl->cardsStatuses.find( _idx );
    if( iter != m_impl->cardsStatuses.end() ){
        SVideoStatus & status = iter->second;

        status.usedByVideoServer = false;
        VS_LOG_INFO << PRINT_HEADER << " release video card with idx [" << _idx << "]" << endl;
    }
    else{
        VS_LOG_WARN << PRINT_HEADER << " no video card to release with such idx [" << _idx << "]" << endl;
    }
}

void SystemMonitor::printOnScreen( const STotalInfo & _info ){

    stringstream ss;

    ss << endl << PRINT_HEADER << "======================== System Monitor ========================" << endl
       << "hostname [" << _info.hostname << "]"
       << " user [" << _info.user << "]"
       << " uptime sec [" << _info.uptimeSec << "]"
       << " RAM total mb [" << _info.memory.ramTotalMb << "]"
       << " RAM free mb [" << _info.memory.ramFreeMb << "]"
       << " swap cached mb [" << _info.memory.swapCached << "]" << endl;

    ss << endl;

    ss << std::setw(15) << std::left << "* CPU core idx"
       << std::setw(50) << std::left << "* name"
       << std::setw(15) << std::left << "* load percent"
       << std::setw(10) << std::left << "* freq MHz"
       << endl;
    for( const SCPUStatus::SCoretatus & core : _info.cpu.cores ){
        ss << std::setw(15) << std::left << core.index
           << std::setw(50) << std::left << core.name
           << std::setw(15) << std::left << core.loadPercent
           << std::setw(10) << std::left << core.frequencyMHz
           << endl;
    }

    ss << endl;

    ss << std::setw(15) << std::left << "* GPU core idx"
       << std::setw(50) << std::left << "* name"
       << std::setw(15) << std::left << "* load percent"
       << std::setw(10) << std::left << "* memory used percent"
       << endl;
    for( const SVideoStatus & video : _info.videocards ){
        ss << std::setw(15) << std::left << video.index
           << std::setw(50) << std::left << video.cardName
           << std::setw(15) << std::left << video.gpuLoadPercent
           << std::setw(10) << std::left << video.memoryUsedPercent
           << endl;
    }

    ss << endl << PRINT_HEADER << "======================== System Monitor ========================" << endl;

    VS_LOG_INFO << ss.str() << endl;
}











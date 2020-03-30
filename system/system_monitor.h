#ifndef SYSTEM_MONITOR_H
#define SYSTEM_MONITOR_H

#include <string>
#include <vector>

class SystemMonitor
{
public:
    static constexpr unsigned int NO_AVAILABLE_CARDS = -1;

    struct SCPUStatus {
        struct SCoretatus {
            SCoretatus()
            {}

            std::string name;
            int index;
            float frequencyMHz;
            float loadPercent;
        };

        SCPUStatus()
        {}
        std::string name;
        float totalLoadPercent;
        float totalLoadByAvgFromCoresPercent;
        std::vector<SCoretatus> cores;
    };

    struct SMemoryStatus {
        SMemoryStatus()
        {}

        int ramTotalMb;
        int ramFreeMb;
        int ramAvailableMb;
        int swapCached;
    };

    struct SVideoStatus {
        SVideoStatus()
            : usedByVideoServer(false)
            , gpuLoadPercent(0)
            , memoryUsedPercent(0)
            , index(0)
        {}
        bool usedByVideoServer;
        unsigned int gpuLoadPercent;
        unsigned int memoryUsedPercent;
        unsigned int index;
        std::string cardName;
    };

    struct STotalInfo {
        STotalInfo()
        {}
        std::string hostname;
        std::string user;
        long uptimeSec;
        SCPUStatus cpu;
        SMemoryStatus memory;
        std::vector<SVideoStatus> videocards;
    };

    //
    static SystemMonitor & singleton(){
        static SystemMonitor instance;
        return instance;
    }

    STotalInfo getTotalSnapshot();

    unsigned int getNextVideoCardIdx();
    void releaseVideoCardIdx( unsigned int _idx );

    void printOnScreen( const STotalInfo & _info );
    const std::string & getLastError();


private:
    SystemMonitor();
    ~SystemMonitor();

    SystemMonitor( const SystemMonitor & _inst ) = delete;
    SystemMonitor & operator=( const SystemMonitor & _inst ) = delete;

    SCPUStatus getCPUStatus();
    SCPUStatus::SCoretatus getCoreStatus( std::stringstream & _ss );
    SMemoryStatus getMemoryStatus();
    std::vector<SVideoStatus> getVideos();
    std::pair<std::string,std::string> getHostnameAndUser();
    long getUptimeSec();

    float getCoreLoad( const std::string & _coreStatLine );
    float getTotalLoad( std::stringstream & _ss );
    std::vector<std::string> getCoreStatLines();

    class SPrivateDataSection * m_impl;
};
#define SYSTEM_MONITOR SystemMonitor::singleton()

#endif // SYSTEM_MONITOR_H

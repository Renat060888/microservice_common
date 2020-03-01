#ifndef LOGGER_COMMON_H
#define LOGGER_COMMON_H

#include <string>

namespace logger_common {

using TFuncPtr = void( * )( const std::string & _msg );

enum class ELogEndpoints {
    Stdout      = 1 << 0,
    File        = 1 << 1,
    MongoDB     = 1 << 2,
    Network     = 1 << 3,
    FuncPtr     = 1 << 4,
    Undefined   = 1 << 31
};

struct SInitSettings {
    SInitSettings() :
        logEndpoints(ELogEndpoints::Stdout)
    {}
    std::string loggerName;
    ELogEndpoints logEndpoints;
    // optional
    TFuncPtr funcPtr;
    std::string fileName;
    std::string filePath;
    int32_t rotationSizeMb;
    std::string unilogConfigPath;
};

}


#endif // LOGGER_COMMON_H

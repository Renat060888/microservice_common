#ifndef LIBRARY_LOADER_H
#define LIBRARY_LOADER_H

#include <map>
#include <string>
#include <vector>

class LibraryLoader
{
public:
    struct SInitSettings {
        std::string directoryForLibrariesSearch;
        std::vector<std::string> filterLibrariesBySymbols;
    };

    struct SLibraryDescr {
        std::string libName;
        void * dlHandler;
        std::map<std::string, void *> dlSymbolsByName;
    };

    LibraryLoader();
    ~LibraryLoader();

    bool init( const SInitSettings & _settings );
    std::vector<std::string> getLoadedLibrariesNames();
    const std::string & getLastError(){ return m_lastError; }

    bool load( const std::string & _libFileName );
    void unload( const std::string & _libFileName );

    void * resolve( const std::string & _libFileName, const std::string & _symbolName );


private:
    bool traverseDirectory( const std::string & _path );


    // data
    std::map<std::string, SLibraryDescr *> m_librariesDescrByName;
    SInitSettings m_settings;
    std::string m_lastError;


    // service



};

#endif // LIBRARY_LOADER_H

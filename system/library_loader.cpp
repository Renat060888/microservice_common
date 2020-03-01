
#include <dlfcn.h>
#include <dirent.h>
#include <sys/stat.h>

#include "library_loader.h"
#include "logger.h"

using namespace std;

static constexpr const char * PRINT_HEADER = "LibLoader:";

static const string PARENT_DIR = "..";
static const string CURRENT_WORKING_DIR = ".";

LibraryLoader::LibraryLoader()
{

}

LibraryLoader::~LibraryLoader(){

    for( auto iter = m_librariesDescrByName.begin(); iter != m_librariesDescrByName.end(); ++iter ){
        SLibraryDescr * libraryDescriptor = iter->second;
        delete libraryDescriptor;
    }

    m_librariesDescrByName.clear();
}

bool LibraryLoader::init( const SInitSettings & _settings ){

    m_settings = _settings;

    if( ! traverseDirectory(_settings.directoryForLibrariesSearch) ){
        return false;
    }

    return true;
}

bool LibraryLoader::traverseDirectory( const std::string & _path ){

    DIR * dir = ::opendir( _path.c_str() );
    if( ! dir ){
        VS_LOG_ERROR << PRINT_HEADER
                     << " cannot open dir [" << _path << "]"
                     << endl;
        return false;
    }

    dirent * de = nullptr;
    while( (de = ::readdir( dir )) != nullptr ){
        const string libName = de->d_name;

        // skip
        if( PARENT_DIR == libName || CURRENT_WORKING_DIR == libName ){
            continue;
        }

        load( libName );
    }

    if( m_librariesDescrByName.empty() ){
        VS_LOG_WARN << PRINT_HEADER
                    << " no one object library is found"
                    << endl;
        ::closedir( dir );
        return true;
    }

    ::closedir( dir );
    return true;
}

std::vector<std::string> LibraryLoader::getLoadedLibrariesNames(){

    std::vector<std::string> out;

    for( auto iter = m_librariesDescrByName.begin(); iter != m_librariesDescrByName.end(); ++iter ){
        SLibraryDescr * libraryDescriptor = iter->second;
        out.push_back( libraryDescriptor->libName );
    }

    return out;
}

bool LibraryLoader::load( const std::string & _libFileName ){

    const string libraryFullPath = m_settings.directoryForLibrariesSearch + "/" + _libFileName;

    // open library
    void * handler = nullptr;
    if( ! (handler = ::dlopen( libraryFullPath.c_str(), RTLD_LOCAL | RTLD_LAZY)) ){
        VS_LOG_ERROR << PRINT_HEADER
                     << " dlopen fail of [" << libraryFullPath << "]"
                     << " Reason [" << ::dlerror() << "]"
                     << endl;
        return false;
    }

    SLibraryDescr * libraryDescriptor = new SLibraryDescr();
    libraryDescriptor->dlHandler = handler;

    // get only filtered by symbols
    if( ! m_settings.filterLibrariesBySymbols.empty() ){

        for( const string & _symbolName : m_settings.filterLibrariesBySymbols ){

            void * runTimeAddress = ::dlsym( handler, _symbolName.c_str() );

            char * error = nullptr;
            if( (error = ::dlerror()) ){
                VS_LOG_WARN << PRINT_HEADER
                            << " dlsym fail of [" << libraryFullPath << "]"
                            << " Reason [" << ::dlerror() << "]"
                            << endl;
                ::dlclose( handler );
                delete libraryDescriptor;
                return false;
            }

            libraryDescriptor->dlSymbolsByName.insert( {_symbolName, runTimeAddress} );
        }

        m_librariesDescrByName.insert( {_libFileName, libraryDescriptor} );
    }
    // get symbols later
    else{
        m_librariesDescrByName.insert( {_libFileName, libraryDescriptor} );
    }

    return true;
}

void LibraryLoader::unload( const std::string & _libFileName ){

    auto iter = m_librariesDescrByName.find( _libFileName );
    if( iter != m_librariesDescrByName.end() ){
        SLibraryDescr * libraryDescriptor = iter->second;

        const int rt = ::dlclose( libraryDescriptor->dlHandler );
        if( rt != 0 ){
            VS_LOG_WARN << PRINT_HEADER
                        << " unmap and closing a shared object failed [" << _libFileName << "]"
                        << " Reason ["  << ::dlerror() << "]"
                        << endl;
        }

        m_librariesDescrByName.erase( iter );
    }
    else{
        VS_LOG_WARN << PRINT_HEADER
                    << " such lib file name not found in colletion [" << _libFileName << "]"
                    << " Call 'load()' first"
                    << endl;
    }
}

void * LibraryLoader::resolve( const std::string & _libFileName, const std::string & _symbolName ){

    auto iter = m_librariesDescrByName.find( _libFileName );
    if( iter != m_librariesDescrByName.end() ){
        SLibraryDescr * libraryDescriptor = iter->second;

        auto iter2 = libraryDescriptor->dlSymbolsByName.find( _symbolName );
        if( iter2 != libraryDescriptor->dlSymbolsByName.end() ){
            return iter2->second;
        }
        else{
            void * runTimeAddress = ::dlsym( libraryDescriptor->dlHandler, _symbolName.c_str() );

            char * error = nullptr;
            if( (error = ::dlerror()) ){
                VS_LOG_WARN << PRINT_HEADER
                            << " dlsym fail of [" << _libFileName << "]"
                            << " Reason [" << ::dlerror()<< "]"
                            << endl;
                return nullptr;
            }

            libraryDescriptor->dlSymbolsByName.insert( {_symbolName, runTimeAddress} );
            return runTimeAddress;
        }
    }
    else{
        VS_LOG_WARN << PRINT_HEADER
                    << " such lib file name not found in colletion [" << _libFileName << "]"
                    << " Call 'load()' first"
                    << endl;
        return nullptr;
    }
}











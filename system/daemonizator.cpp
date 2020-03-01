
#include <signal.h>
#include <unistd.h>
#include <string.h>

#include "daemonizator.h"
#include "logger.h"

using namespace std;

Daemonizator::Daemonizator()
{

}

bool Daemonizator::turnIntoDaemon(){

    VS_LOG_INFO << "try to become a deamon, see log" << endl;

    // auto mode
    constexpr int dontChangeDir = 0;
    constexpr int dontCloseIO = 0;
    if( EXIT_SUCCESS == ::daemon(dontChangeDir, dontCloseIO) ){
        return true;
    }
    else{
        VS_LOG_ERROR << "daemon() failed, reason: " << ::strerror( errno ) << endl;
        return false;
    }

    // manual mode
//    int retCode = fork();
//    if( 0 == retCode ){
//        LOG_TRACE << "THIS IS CHILD" << endl;
//        // file creation mask
//        umask( 0 );
//        // create new session and become leader
//        setsid();
//        // reset dir
//        chdir("/");
//        // close std io
//        close( STDIN_FILENO );
//        close( STDOUT_FILENO );
//        close( STDERR_FILENO );
//        LOG_INFO( "video server daemon started with pid [{}]", getpid() );
//    }
//    else if( retCode > 0 ){
//        LOG_TRACE << "THIS IS PARENT" << endl;
//        LOG_INFO( "parent process creates child with pid [{}]", retCode );
//        exit( EXIT_SUCCESS );
//    }
//    else{
//        LOG_CRITICAL("fork failed");
//        return false;
//    }

    return true;
}






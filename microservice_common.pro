ROOT_DIR=../

TEMPLATE = lib
TARGET = microservice_common

include($${ROOT_DIR}pri/common.pri)

CONFIG -= qt
CONFIG += plugin
CONFIG += link_pkgconfig
#CONFIG += release

PKGCONFIG += glib-2.0

QMAKE_CXXFLAGS += -Wno-unused-parameter
QMAKE_CXXFLAGS += -Wno-unused-variable

# TODO: add defines to logger, system monitor, restbed webserver, database, etc...
DEFINES += \
    SWITCH_LOGGER_SIMPLE \
#    SWITCH_LOGGER_ASTRA \
#    OBJREPR_LIBRARY_EXIST \
    UNIT_TESTS_GOOGLE \

INCLUDEPATH += \
    /usr/include/libgtop-2.0 \
    /usr/include/libmongoc-1.0 \
    /usr/include/libbson-1.0 \

LIBS += -L/usr/lib/x86_64-linux-gnu/nvidia/current
LIBS += \    
    -lmongoc-1.0 \
    -lbson-1.0 \
    -lcurl \
    -lrabbitmq \
    -lnvidia-ml \
    -lgtop-2.0 \
    -lboost_filesystem \
    -lboost_program_options \

contains( DEFINES, OBJREPR_LIBRARY_EXIST ){
    message("connect 'unilog' and 'objrepr' libraries")
LIBS += \    
    -lunilog \
    -lobjrepr
}

contains( DEFINES, UNIT_TESTS_GOOGLE ){
    message("connect 'gtests' library")
LIBS += \
    -lgtest
}

SOURCES += \
        3rd_party/EdUrlParser.cpp \
        3rd_party/base64.cpp \
        3rd_party/mongoose.c \
        common/error_entity.cpp \
        common/ms_common_types.cpp \
        communication/amqp_client_c.cpp \
        communication/amqp_controller.cpp \
        communication/communication_gateway_facade.cpp \
        communication/http_client.cpp \
        communication/i_command.cpp \
        communication/i_command_external.cpp \
        communication/i_command_factory.cpp \
        communication/network_interface.cpp \
        communication/objrepr_listener.cpp \
        communication/shared_memory_server.cpp \
        communication/shell.cpp \
        communication/unified_command_convertor.cpp \
        communication/webserver.cpp \
        communication/websocket_server.cpp \
        storage/database_manager_base.cpp \
        system/a_args_parser.cpp \
        system/a_config_reader.cpp \
        system/daemonizator.cpp \
        system/library_loader.cpp \
        system/logger_astra.cpp \
        system/logger_normal.cpp \
        system/logger_simple.cpp \
        system/object_pool.cpp \
        system/objrepr_bus.cpp \
        system/process_launcher.cpp \
        system/system_monitor.cpp \
        system/thread_pool.cpp \
        system/threaded_multitask_service.cpp \
        system/wal.cpp \
        unit_tests/communication_tests.cpp \
        unit_tests/database_manager_base_test.cpp \
        unit_tests/storage_tests.cpp \
        unit_tests/system_tests.cpp \
    system/system_environment_facade.cpp \
    communication/network_splitter.cpp

HEADERS += \
    3rd_party/EdUrlParser.h \
    3rd_party/base64.h \
    3rd_party/mongoose.h \
    3rd_party/string_compression.h \
    common/error_entity.h \
    common/ms_common_types.h \
    common/ms_common_utils.h \
    common/ms_common_vars.h \
    communication/amqp_client_c.h \
    communication/amqp_controller.h \
    communication/communication_gateway_facade.h \
    communication/http_client.h \
    communication/i_command.h \
    communication/i_command_external.h \
    communication/i_command_factory.h \
    communication/network_interface.h \
    communication/objrepr_listener.h \
    communication/shared_memory_server.h \
    communication/shell.h \
    communication/unified_command_convertor.h \
    communication/webserver.h \
    communication/websocket_server.h \
    datasource/dummy.h \
    storage/database_manager_base.h \
    system/a_args_parser.h \
    system/a_config_reader.h \
    system/class_factory.h \
    system/daemonizator.h \
    system/library_loader.h \
    system/logger.h \
    system/logger_astra.h \
    system/logger_common.h \
    system/logger_normal.h \
    system/logger_simple.h \
    system/object_pool.h \
    system/objrepr_bus.h \
    system/process_launcher.h \
    system/system_monitor.h \
    system/thread_pool.h \
    system/thread_pool_task.h \
    system/threaded_multitask_service.h \
    system/wal.h \
    unit_tests/communication_tests.h \
    unit_tests/database_manager_base_test.h \
    unit_tests/storage_tests.h \
    unit_tests/system_tests.h \
    analyze/dummy.h \
    system/system_environment_facade.h \
    communication/network_splitter.h

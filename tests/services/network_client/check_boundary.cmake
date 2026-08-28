# SPDX-License-Identifier: GPL-3.0-or-later

if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT is required")
endif()

foreach(module IN ITEMS network_protocol network_model network_client)
    file(
        GLOB_RECURSE network_sources
        LIST_DIRECTORIES false
        "${SOURCE_ROOT}/src/services/${module}/*.h"
        "${SOURCE_ROOT}/src/services/${module}/*.cpp"
    )
    if(EXISTS "${SOURCE_ROOT}/src/services/${module}/CMakeLists.txt")
        list(APPEND network_sources
             "${SOURCE_ROOT}/src/services/${module}/CMakeLists.txt")
    endif()

    foreach(source IN LISTS network_sources)
        file(READ "${source}" content)
        # N0 is a pure boundary: no NetworkManager/BlueZ stack, no bus, no
        # sockets, no QML, no credential storage, no process/file reach, and
        # no wall-clock/timer authority inside the pure model.
        if(content MATCHES "NetworkManager|BluezQt|bluez"
           OR content MATCHES "<QtDBus/" OR content MATCHES "<QtQml/"
           OR content MATCHES "<QtQuick/"
           OR content MATCHES "QTcpSocket|QUdpSocket|QNetworkAccessManager|QHostAddress|QLocalSocket"
           OR content MATCHES "libsecret|keyring|QSslKey|QSslCertificate")
            message(FATAL_ERROR "Forbidden network-${module} dependency in ${source}")
        endif()
        if(module STREQUAL "network_model" AND
           (content MATCHES "<QtCore/Q(File|Settings|Timer|ElapsedTimer|Object|Process|Thread)"
            OR content MATCHES "std::thread|std::filesystem|fopen\\(|std::ofstream"))
            message(FATAL_ERROR "Forbidden impurity in network model ${source}")
        endif()
        if(module STREQUAL "network_protocol" AND
           content MATCHES "<QtCore/Q(File|Settings|Timer|ElapsedTimer|Object|Process|Thread)")
            message(FATAL_ERROR "Forbidden impurity in network protocol ${source}")
        endif()
    endforeach()
endforeach()

message(STATUS "Network N0 boundary is stack, bus, GUI, and secret independent")

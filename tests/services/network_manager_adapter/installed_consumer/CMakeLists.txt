# SPDX-License-Identifier: GPL-3.0-or-later

cmake_minimum_required(VERSION 3.25)
project(QindaQtInstalledNetworkN1Consumer LANGUAGES CXX)

find_package(Qt6 6.11 REQUIRED COMPONENTS Core DBus)
find_package(PkgConfig REQUIRED)
pkg_check_modules(NetworkManager REQUIRED IMPORTED_TARGET libnm>=1.44)

foreach(required IN ITEMS
        QINDAQT_STAGE_INCLUDE_DIR
        QINDAQT_NETWORK_PROTOCOL_LIBRARY
        QINDAQT_NETWORK_MODEL_LIBRARY
        QINDAQT_NETWORK_CLIENT_LIBRARY
        QINDAQT_NETWORK_QT_TRANSPORT_LIBRARY
        QINDAQT_NETWORK_SERVICE_LIBRARY
        QINDAQT_NETWORK_MANAGER_ADAPTER_LIBRARY)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "Missing installed N1 consumer input: ${required}")
    endif()
endforeach()

function(import_archive target location)
    add_library(${target} STATIC IMPORTED GLOBAL)
    set_target_properties(
        ${target}
        PROPERTIES
            IMPORTED_LOCATION "${location}"
            INTERFACE_INCLUDE_DIRECTORIES "${QINDAQT_STAGE_INCLUDE_DIR}"
    )
endfunction()

import_archive(QindaQtInstalledNetworkProtocol "${QINDAQT_NETWORK_PROTOCOL_LIBRARY}")
target_link_libraries(QindaQtInstalledNetworkProtocol INTERFACE Qt6::Core)
import_archive(QindaQtInstalledNetworkModel "${QINDAQT_NETWORK_MODEL_LIBRARY}")
target_link_libraries(QindaQtInstalledNetworkModel
                      INTERFACE QindaQtInstalledNetworkProtocol Qt6::Core)
import_archive(QindaQtInstalledNetworkClient "${QINDAQT_NETWORK_CLIENT_LIBRARY}")
target_link_libraries(QindaQtInstalledNetworkClient
                      INTERFACE QindaQtInstalledNetworkModel Qt6::Core)
import_archive(QindaQtInstalledNetworkQtTransport
               "${QINDAQT_NETWORK_QT_TRANSPORT_LIBRARY}")
target_link_libraries(QindaQtInstalledNetworkQtTransport
                      INTERFACE QindaQtInstalledNetworkClient Qt6::Core Qt6::DBus)
import_archive(QindaQtInstalledNetworkService "${QINDAQT_NETWORK_SERVICE_LIBRARY}")
target_link_libraries(QindaQtInstalledNetworkService
                      INTERFACE QindaQtInstalledNetworkModel Qt6::Core Qt6::DBus)
import_archive(QindaQtInstalledNetworkManagerAdapter
               "${QINDAQT_NETWORK_MANAGER_ADAPTER_LIBRARY}")
target_link_libraries(
    QindaQtInstalledNetworkManagerAdapter
    INTERFACE QindaQtInstalledNetworkService PkgConfig::NetworkManager Qt6::Core
)

add_executable(qindaqt_installed_network_n1_consumer installed_network_n1_consumer.cpp)
target_compile_features(qindaqt_installed_network_n1_consumer PRIVATE cxx_std_20)
set_target_properties(qindaqt_installed_network_n1_consumer
                      PROPERTIES CXX_EXTENSIONS OFF)
target_link_libraries(
    qindaqt_installed_network_n1_consumer
    PRIVATE
        QindaQtInstalledNetworkQtTransport
        QindaQtInstalledNetworkManagerAdapter
)

# SPDX-License-Identifier: GPL-3.0-or-later

cmake_minimum_required(VERSION 3.25)
project(QindaQtInstalledNetworkConsumer LANGUAGES CXX)

find_package(Qt6 6.11 REQUIRED COMPONENTS Core)

foreach(required IN ITEMS QINDAQT_STAGE_INCLUDE_DIR QINDAQT_NETWORK_PROTOCOL_LIBRARY
                          QINDAQT_NETWORK_MODEL_LIBRARY QINDAQT_NETWORK_CLIENT_LIBRARY
                          QINDAQT_CONSUMER_SOURCE)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "Missing staged-consumer input: ${required}")
    endif()
endforeach()

add_library(QindaQtInstalledNetworkProtocol STATIC IMPORTED GLOBAL)
set_target_properties(
    QindaQtInstalledNetworkProtocol
    PROPERTIES
        IMPORTED_LOCATION "${QINDAQT_NETWORK_PROTOCOL_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${QINDAQT_STAGE_INCLUDE_DIR}"
)
target_link_libraries(QindaQtInstalledNetworkProtocol INTERFACE Qt6::Core)

add_library(QindaQtInstalledNetworkModel STATIC IMPORTED GLOBAL)
set_target_properties(
    QindaQtInstalledNetworkModel
    PROPERTIES
        IMPORTED_LOCATION "${QINDAQT_NETWORK_MODEL_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${QINDAQT_STAGE_INCLUDE_DIR}"
)
target_link_libraries(QindaQtInstalledNetworkModel
                      INTERFACE QindaQtInstalledNetworkProtocol Qt6::Core)

add_library(QindaQtInstalledNetworkClient STATIC IMPORTED GLOBAL)
set_target_properties(
    QindaQtInstalledNetworkClient
    PROPERTIES
        IMPORTED_LOCATION "${QINDAQT_NETWORK_CLIENT_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${QINDAQT_STAGE_INCLUDE_DIR}"
)
target_link_libraries(QindaQtInstalledNetworkClient
                      INTERFACE QindaQtInstalledNetworkModel Qt6::Core)

add_executable(qindaqt_installed_network_consumer "${QINDAQT_CONSUMER_SOURCE}")
target_compile_features(qindaqt_installed_network_consumer PRIVATE cxx_std_20)
set_target_properties(
    qindaqt_installed_network_consumer PROPERTIES CXX_EXTENSIONS OFF
)
target_link_libraries(qindaqt_installed_network_consumer
                      PRIVATE QindaQtInstalledNetworkClient)

# SPDX-License-Identifier: GPL-3.0-or-later

cmake_minimum_required(VERSION 3.25)
project(QindaQtInstalledPowerConsumer LANGUAGES CXX)

find_package(Qt6 6.11 REQUIRED COMPONENTS Core DBus)

foreach(required IN ITEMS QINDAQT_STAGE_INCLUDE_DIR QINDAQT_POWER_CLIENT_LIBRARY
                          QINDAQT_POWER_SERVICE_LIBRARY QINDAQT_POWER_PROTOCOL_LIBRARY
                          QINDAQT_CONSUMER_SOURCE)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "Missing staged-consumer input: ${required}")
    endif()
endforeach()

add_library(QindaQtInstalledPowerProtocol STATIC IMPORTED GLOBAL)
set_target_properties(
    QindaQtInstalledPowerProtocol
    PROPERTIES IMPORTED_LOCATION "${QINDAQT_POWER_PROTOCOL_LIBRARY}"
)
add_library(QindaQtInstalledPowerService STATIC IMPORTED GLOBAL)
set_target_properties(
    QindaQtInstalledPowerService
    PROPERTIES
        IMPORTED_LOCATION "${QINDAQT_POWER_SERVICE_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${QINDAQT_STAGE_INCLUDE_DIR}"
        INTERFACE_LINK_LIBRARIES QindaQtInstalledPowerProtocol
)
add_library(QindaQtInstalledPowerClient STATIC IMPORTED GLOBAL)
set_target_properties(
    QindaQtInstalledPowerClient
    PROPERTIES
        IMPORTED_LOCATION "${QINDAQT_POWER_CLIENT_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${QINDAQT_STAGE_INCLUDE_DIR}"
        INTERFACE_LINK_LIBRARIES QindaQtInstalledPowerProtocol
)

add_executable(qindaqt_installed_power_consumer "${QINDAQT_CONSUMER_SOURCE}")
target_compile_features(qindaqt_installed_power_consumer PRIVATE cxx_std_20)
set_target_properties(qindaqt_installed_power_consumer PROPERTIES CXX_EXTENSIONS OFF)
target_link_libraries(
    qindaqt_installed_power_consumer
    PRIVATE QindaQtInstalledPowerClient QindaQtInstalledPowerService
            Qt6::Core Qt6::DBus
)

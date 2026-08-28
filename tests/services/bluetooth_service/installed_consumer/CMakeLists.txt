# SPDX-License-Identifier: GPL-3.0-or-later

cmake_minimum_required(VERSION 3.25)
project(QindaQtInstalledBluetoothConsumer LANGUAGES CXX)

find_package(Qt6 6.11 REQUIRED COMPONENTS Core DBus)

foreach(required IN ITEMS QINDAQT_STAGE_INCLUDE_DIR QINDAQT_BLUETOOTH_PROTOCOL_LIBRARY
                          QINDAQT_CONSUMER_SOURCE)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "Missing staged-consumer input: ${required}")
    endif()
endforeach()

add_library(QindaQtInstalledBluetoothProtocol STATIC IMPORTED GLOBAL)
set_target_properties(
    QindaQtInstalledBluetoothProtocol
    PROPERTIES
        IMPORTED_LOCATION "${QINDAQT_BLUETOOTH_PROTOCOL_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${QINDAQT_STAGE_INCLUDE_DIR}"
)
target_link_libraries(
    QindaQtInstalledBluetoothProtocol INTERFACE Qt6::Core Qt6::DBus
)

add_executable(qindaqt_installed_bluetooth_consumer "${QINDAQT_CONSUMER_SOURCE}")
target_compile_features(qindaqt_installed_bluetooth_consumer PRIVATE cxx_std_20)
set_target_properties(
    qindaqt_installed_bluetooth_consumer PROPERTIES CXX_EXTENSIONS OFF
)
target_link_libraries(
    qindaqt_installed_bluetooth_consumer PRIVATE QindaQtInstalledBluetoothProtocol
)

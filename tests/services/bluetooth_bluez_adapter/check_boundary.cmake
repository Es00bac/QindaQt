# SPDX-License-Identifier: GPL-3.0-or-later

if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT is required")
endif()

set(adapter_root "${SOURCE_ROOT}/src/services/bluetooth_bluez_adapter")
set(adapter_sources_root "${SOURCE_ROOT}/src/services/bluetooth_bluez_adapter/src")
set(adapter_public_headers
    "${SOURCE_ROOT}/src/services/bluetooth_bluez_adapter/include")
set(service_main "${SOURCE_ROOT}/src/services/bluetooth_service/app/main.cpp")
set(service_cmake "${SOURCE_ROOT}/src/services/bluetooth_service/CMakeLists.txt")
set(adapter_cmake "${adapter_root}/CMakeLists.txt")
set(adapter_tests "${SOURCE_ROOT}/tests/services/bluetooth_bluez_adapter")

# Production adapter sources: no BluezQt, no residency/service internals, no
# QML/shell, and no BlueZ mutation outside the accepted v1 operation set.
file(GLOB_RECURSE sources LIST_DIRECTORIES false
     "${adapter_root}/*.h" "${adapter_root}/*.cpp")
foreach(source IN LISTS sources)
    file(READ "${source}" content)
    if(content MATCHES "BluezQt|bluez-qt|KF6BluezQt")
        message(FATAL_ERROR "BlueZ adapter reaches BlueZ through BluezQt in ${source}")
    endif()
    if(content MATCHES "services/bluetooth_service/|resident_bluetooth_service")
        message(FATAL_ERROR "BlueZ adapter reverses module direction in ${source}")
    endif()
    if(content MATCHES "#include <QtQuick|#include <QtQml|services/settings|src/shell/")
        message(FATAL_ERROR "BlueZ adapter crosses into shell/QML/settings in ${source}")
    endif()
    if(content MATCHES "QStringLiteral\\(\"(Pair|Trust|Untrust|RemoveDevice|CancelPairing|SetPairable|SetDiscoveryFilter)\"\\)")
        message(FATAL_ERROR "BlueZ adapter claims BlueZ pairing/trust/record authority in ${source}")
    endif()
    if(content MATCHES "/dev/rfkill|QProcess")
        message(FATAL_ERROR "BlueZ adapter touches radios or spawns processes in ${source}")
    endif()
endforeach()

# The public adapter headers must not leak transport or platform internals.
file(GLOB_RECURSE public_headers LIST_DIRECTORIES false
     "${adapter_public_headers}/*.h")
foreach(source IN LISTS public_headers)
    file(READ "${source}" content)
    if(content MATCHES "bluez_transport|bluez_object_store|BluezManagedObjects")
        message(FATAL_ERROR "Public BlueZ adapter header leaks private internals in ${source}")
    endif()
    if(content MATCHES "BluezQt|bluez-qt")
        message(FATAL_ERROR "Public BlueZ adapter header names BluezQt in ${source}")
    endif()
endforeach()

# Tests must inject their own bus; a host system-bus or session-bus contact
# from a test is an environment escape.
file(GLOB_RECURSE test_sources LIST_DIRECTORIES false
     "${adapter_tests}/*.h" "${adapter_tests}/*.cpp")
foreach(source IN LISTS test_sources)
    file(READ "${source}" content)
    if(content MATCHES "QDBusConnection::systemBus|QDBusConnection::sessionBus")
        message(FATAL_ERROR "BlueZ adapter test contacts an ambient bus in ${source}")
    endif()
    if(content MATCHES "QProcess::start\\(\"bluetoothctl|system\\(\"bluetoothctl")
        message(FATAL_ERROR "BlueZ adapter test escapes to host tooling in ${source}")
    endif()
endforeach()

# The composition root must select the backend explicitly and keep the
# deterministic escape hatch available (ADR-0056). These files exist on the
# real tree; a poison tree exercises one of them at a time.
if(EXISTS "${service_main}")
    file(READ "${service_main}" main_content)
    if(NOT main_content MATCHES "QINDAQT_BLUETOOTH_BACKEND"
       OR NOT main_content MATCHES "resolveBluetoothBackendMode"
       OR NOT main_content MATCHES "BluezAdapterBackend"
       OR NOT main_content MATCHES "makeDeterministicAdapterBackend")
        message(FATAL_ERROR "Bluetooth1 composition root lost explicit backend selection")
    endif()
    if(NOT main_content MATCHES "QDBusConnection::systemBus")
        message(FATAL_ERROR "Bluetooth1 production mode must consume org.bluez on the system bus")
    endif()
endif()

if(EXISTS "${service_cmake}")
    file(READ "${service_cmake}" service_cmake_content)
    if(NOT service_cmake_content MATCHES "QindaQt::BluezAdapter")
        message(FATAL_ERROR "Bluetooth1 service composition does not link the production adapter")
    endif()
endif()

if(EXISTS "${adapter_cmake}")
    file(READ "${adapter_cmake}" adapter_cmake_content)
    if(NOT adapter_cmake_content MATCHES "QindaQt::BluetoothModel"
       OR NOT adapter_cmake_content MATCHES "COMPONENT QindaQtBluetoothB1")
        message(FATAL_ERROR "BlueZ adapter package registry is incomplete")
    endif()
endif()

message(STATUS "Bluetooth B1 keeps adapter, port, composition, and test boundaries separated")

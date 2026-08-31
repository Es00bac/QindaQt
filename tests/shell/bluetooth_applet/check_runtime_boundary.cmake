# SPDX-License-Identifier: GPL-3.0-or-later

if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "Missing Bluetooth applet runtime source root")
endif()

set(runtime_root "${SOURCE_ROOT}/src/shell/bluetooth_applet")
set(applet_runtime_sources
    "${runtime_root}/src/bluetooth_applet_controller.h"
    "${runtime_root}/src/bluetooth_applet_controller.cpp"
    "${runtime_root}/qml/BluetoothAdapterRow.qml"
    "${runtime_root}/qml/BluetoothApplet.qml"
    "${runtime_root}/qml/BluetoothDeviceRow.qml")
set(composition_sources
    "${SOURCE_ROOT}/src/shell/runtime/bluetoothappletcomposition.h"
    "${SOURCE_ROOT}/src/shell/runtime/bluetoothappletcomposition.cpp")
set(runtime_sources ${applet_runtime_sources} ${composition_sources})
list(LENGTH runtime_sources runtime_source_count)

set(forbidden_everywhere
    "bluetooth_service"
    "bluetooth_model"
    "BlueZ"
    "BluezQt"
    "Agent1"
    "PairDevice"
    "TrustDevice"
    "UntrustDevice"
    ".address"
    "QFile"
    "QProcess"
    "QNetwork")
set(violations "")
foreach(path IN LISTS runtime_sources)
    if(NOT EXISTS "${path}")
        list(APPEND violations "missing runtime boundary source ${path}")
        continue()
    endif()
    file(READ "${path}" content)
    foreach(token IN LISTS forbidden_everywhere)
        string(FIND "${content}" "${token}" hit)
        if(NOT hit EQUAL -1)
            list(APPEND violations "${path}: forbidden token '${token}'")
        endif()
    endforeach()
endforeach()

# Only the production composition root may construct the public Qt transport.
# The controller and renderer never see transport or D-Bus vocabulary.
foreach(path IN LISTS applet_runtime_sources)
    file(READ "${path}" content)
    foreach(token IN ITEMS "QtBluetoothTransport" "QtDBus" "QDBus")
        string(FIND "${content}" "${token}" hit)
        if(NOT hit EQUAL -1)
            list(APPEND violations "${path}: forbidden token '${token}'")
        endif()
    endforeach()
endforeach()

# Audit the actual production composition chain rather than accepting a
# manifest-only or source-only applet.
set(required_contracts
    "${SOURCE_ROOT}/data/applets/bluetooth.json|\"qindaqt.applets.bluetooth\""
    "${SOURCE_ROOT}/data/applets/bluetooth.json|\"bluetooth.read\""
    "${SOURCE_ROOT}/data/applets/bluetooth.json|\"bluetooth.control\""
    "${SOURCE_ROOT}/src/applet_runtime/src/builtin_applet_registry.cpp|qindaqt.applets.bluetooth"
    "${SOURCE_ROOT}/src/shell/qml/BuiltinAppletContent.qml|QindaQt.Shell.BluetoothApplet"
    "${SOURCE_ROOT}/src/shell/qml/BuiltinAppletContent.qml|qindaqt.applets.bluetooth"
    "${SOURCE_ROOT}/src/shell/runtime/bluetoothappletcomposition.cpp|BluetoothRead"
    "${SOURCE_ROOT}/src/shell/runtime/bluetoothappletcomposition.cpp|BluetoothControl"
    "${SOURCE_ROOT}/src/shell/runtime/shellruntimeapplication.cpp|m_bluetoothApplet"
    "${SOURCE_ROOT}/data/profiles/qindaqt.json|\"plugin\": \"bluetooth\"")
foreach(contract IN LISTS required_contracts)
    string(REPLACE "|" ";" fields "${contract}")
    list(GET fields 0 path)
    list(GET fields 1 token)
    if(NOT EXISTS "${path}")
        list(APPEND violations "missing composition path ${path}")
    else()
        file(READ "${path}" content)
        string(FIND "${content}" "${token}" hit)
        if(hit EQUAL -1)
            list(APPEND violations "${path}: missing production token '${token}'")
        endif()
    endif()
endforeach()

if(violations)
    foreach(violation IN LISTS violations)
        message(SEND_ERROR "${violation}")
    endforeach()
    message(FATAL_ERROR "Bluetooth applet runtime boundary failed")
endif()

# Mutation-sensitive negative control rejects both service reach and a pairing
# invokable planted inside the shell-private controller.
if(DEFINED POISON_ROOT AND NOT BLUETOOTH_RUNTIME_POLICY_SKIP_POISON)
    cmake_path(NORMAL_PATH POISON_ROOT OUTPUT_VARIABLE poison_root)
    file(REMOVE_RECURSE "${poison_root}")
    file(MAKE_DIRECTORY
         "${poison_root}/src/shell"
         "${poison_root}/src/shell/runtime"
         "${poison_root}/src/applet_runtime/src"
         "${poison_root}/src/shell/qml"
         "${poison_root}/data/applets"
         "${poison_root}/data/profiles")
    file(COPY "${runtime_root}" DESTINATION "${poison_root}/src/shell")
    file(COPY ${composition_sources}
         DESTINATION "${poison_root}/src/shell/runtime")
    file(COPY
         "${SOURCE_ROOT}/src/applet_runtime/src/builtin_applet_registry.cpp"
         DESTINATION "${poison_root}/src/applet_runtime/src")
    file(COPY
         "${SOURCE_ROOT}/src/shell/qml/BuiltinAppletContent.qml"
         DESTINATION "${poison_root}/src/shell/qml")
    file(COPY
         "${SOURCE_ROOT}/src/shell/runtime/shellruntimeapplication.cpp"
         DESTINATION "${poison_root}/src/shell/runtime")
    file(COPY "${SOURCE_ROOT}/data/applets/bluetooth.json"
         DESTINATION "${poison_root}/data/applets")
    file(COPY "${SOURCE_ROOT}/data/profiles/qindaqt.json"
         DESTINATION "${poison_root}/data/profiles")
    file(APPEND
         "${poison_root}/src/shell/bluetooth_applet/src/bluetooth_applet_controller.cpp"
         "\n#include <qindaqt/services/bluetooth_service/resident_bluetooth_service.h>\nPairDevice();\n")
    execute_process(
        COMMAND "${CMAKE_COMMAND}"
                "-DSOURCE_ROOT=${poison_root}"
                -DBLUETOOTH_RUNTIME_POLICY_SKIP_POISON=ON
                -P "${CMAKE_CURRENT_LIST_FILE}"
        RESULT_VARIABLE poison_status
        OUTPUT_VARIABLE poison_output
        ERROR_VARIABLE poison_error)
    file(REMOVE_RECURSE "${poison_root}")
    if(poison_status EQUAL 0)
        message(FATAL_ERROR
            "Bluetooth runtime boundary accepted service/pairing poison:\n"
            "${poison_output}${poison_error}")
    endif()
endif()

message(STATUS
    "Bluetooth applet runtime boundary passed (${runtime_source_count} files and poison rejection)")

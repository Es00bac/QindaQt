# SPDX-License-Identifier: LGPL-3.0-or-later

if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "Missing Bluetooth applet source root")
endif()

set(applet_root "${SOURCE_ROOT}/src/shell/bluetooth_applet")
set(pure_sources
    "${applet_root}/include/qindaqt/shell/bluetooth_applet/bluetooth_applet_presentation.h"
    "${applet_root}/include/qindaqt/shell/bluetooth_applet/bluetooth_applet_types.h"
    "${applet_root}/include/qindaqt/shell/bluetooth_applet/bluetooth_request_state.h"
    "${applet_root}/src/bluetooth_applet_presentation.cpp"
    "${applet_root}/src/bluetooth_request_state.cpp")
list(LENGTH pure_sources pure_source_count)

set(allowed_include_roots
    "qindaqt/shell/bluetooth_applet/"
    "qindaqt/services/bluetooth_protocol/")
set(forbidden
    "BluetoothClient"
    "bluetooth_client"
    "bluetooth_model"
    "bluetooth_service"
    "QtDBus"
    "QtQml"
    "QtQuick"
    "QtGui"
    "QtWidgets"
    "QObject"
    "QTimer"
    "QFile"
    "QProcess"
    "QNetwork"
    "QDBus"
    "BlueZ"
    "BluezQt"
    "Agent1"
    "PairDevice"
    "TrustDevice"
    ".address")

set(violations "")
foreach(path IN LISTS pure_sources)
    if(NOT EXISTS "${path}")
        list(APPEND violations "missing pure boundary source ${path}")
        continue()
    endif()
    file(READ "${path}" content)
    foreach(token IN LISTS forbidden)
        string(FIND "${content}" "${token}" hit)
        if(NOT hit EQUAL -1)
            list(APPEND violations "${path}: forbidden token '${token}'")
        endif()
    endforeach()
    string(REGEX MATCHALL "#[ \t]*include[ \t]*[<\"]([^\">]+)" includes "${content}")
    foreach(include_line IN LISTS includes)
        string(REGEX REPLACE "#[ \t]*include[ \t]*[<\"]" "" include_path "${include_line}")
        set(allowed FALSE)
        foreach(root IN LISTS allowed_include_roots)
            string(FIND "${include_path}" "${root}" position)
            if(position EQUAL 0)
                set(allowed TRUE)
                break()
            endif()
        endforeach()
        if(NOT allowed)
            string(FIND "${include_path}" "QtCore/" position)
            if(position EQUAL 0)
                set(allowed TRUE)
            endif()
        endif()
        if(NOT allowed)
            string(FIND "${include_path}" "/" position)
            if(position EQUAL -1)
                set(allowed TRUE)
            endif()
        endif()
        if(NOT allowed)
            list(APPEND violations "${path}: non-boundary include '${include_path}'")
        endif()
    endforeach()
endforeach()

if(violations)
    foreach(violation IN LISTS violations)
        message(SEND_ERROR "${violation}")
    endforeach()
    message(FATAL_ERROR "Bluetooth applet pure boundary failed")
endif()

# Mutation-sensitive negative control: the same policy must reject a planted
# public-client dependency inside the pure target.
if(DEFINED POISON_ROOT AND NOT BLUETOOTH_PURE_POLICY_SKIP_POISON)
    cmake_path(NORMAL_PATH POISON_ROOT OUTPUT_VARIABLE poison_root)
    file(REMOVE_RECURSE "${poison_root}")
    file(MAKE_DIRECTORY "${poison_root}/src/shell")
    file(COPY "${applet_root}" DESTINATION "${poison_root}/src/shell")
    file(APPEND
         "${poison_root}/src/shell/bluetooth_applet/include/qindaqt/shell/bluetooth_applet/bluetooth_applet_types.h"
         "#include <qindaqt/services/bluetooth_client/bluetooth_client.h>\n")
    execute_process(
        COMMAND "${CMAKE_COMMAND}"
                "-DSOURCE_ROOT=${poison_root}"
                -DBLUETOOTH_PURE_POLICY_SKIP_POISON=ON
                -P "${CMAKE_CURRENT_LIST_FILE}"
        RESULT_VARIABLE poison_status
        OUTPUT_VARIABLE poison_output
        ERROR_VARIABLE poison_error)
    file(REMOVE_RECURSE "${poison_root}")
    if(poison_status EQUAL 0)
        message(FATAL_ERROR
            "Bluetooth pure boundary accepted client poison:\n"
            "${poison_output}${poison_error}")
    endif()
endif()

message(STATUS
    "Bluetooth applet pure boundary passed (${pure_source_count} files and poison rejection)")

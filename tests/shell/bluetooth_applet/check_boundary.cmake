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

set(allowed_includes
    "qindaqt/shell/bluetooth_applet/bluetooth_applet_presentation.h"
    "qindaqt/shell/bluetooth_applet/bluetooth_applet_types.h"
    "qindaqt/shell/bluetooth_applet/bluetooth_request_state.h"
    "qindaqt/services/bluetooth_protocol/bluetooth_limits.h"
    "qindaqt/services/bluetooth_protocol/bluetooth_types.h"
    "qindaqt/services/bluetooth_protocol/bluetooth_validation.h"
    "QtCore/QList"
    "QtCore/QString"
    "QtCore/QStringList"
    "algorithm"
    "optional")
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
        if(NOT include_path IN_LIST allowed_includes)
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

# Mutation-sensitive negative controls independently prove that the exact
# header allowlist rejects public-client, persistence, filesystem, and adjacent
# Qt-module reach.
if(DEFINED POISON_ROOT AND NOT BLUETOOTH_PURE_POLICY_SKIP_POISON)
    cmake_path(NORMAL_PATH POISON_ROOT OUTPUT_VARIABLE poison_root)
    file(REMOVE_RECURSE "${poison_root}")

    function(expect_pure_poison_rejected name relative_path poison_content)
        set(case_root "${poison_root}/${name}")
        file(MAKE_DIRECTORY "${case_root}/src/shell")
        file(COPY "${applet_root}" DESTINATION "${case_root}/src/shell")
        file(APPEND "${case_root}/${relative_path}" "${poison_content}")
        execute_process(
            COMMAND "${CMAKE_COMMAND}"
                    "-DSOURCE_ROOT=${case_root}"
                    -DBLUETOOTH_PURE_POLICY_SKIP_POISON=ON
                    -P "${CMAKE_CURRENT_LIST_FILE}"
            RESULT_VARIABLE poison_status
            OUTPUT_VARIABLE poison_output
            ERROR_VARIABLE poison_error)
        if(poison_status EQUAL 0)
            message(FATAL_ERROR
                "Bluetooth pure boundary accepted ${name} poison:\n"
                "${poison_output}${poison_error}")
        endif()
    endfunction()

    set(types_path
        "src/shell/bluetooth_applet/include/qindaqt/shell/bluetooth_applet/bluetooth_applet_types.h")
    set(presentation_path
        "src/shell/bluetooth_applet/src/bluetooth_applet_presentation.cpp")
    expect_pure_poison_rejected(
        "public-client" "${types_path}"
        "#include <qindaqt/services/bluetooth_client/bluetooth_client.h>\n")
    expect_pure_poison_rejected(
        "persistence" "${types_path}" "#include <QtCore/QSettings>\n")
    expect_pure_poison_rejected(
        "filesystem" "${presentation_path}" "#include <filesystem>\n")
    expect_pure_poison_rejected(
        "adjacent-network" "${presentation_path}"
        "#include <QtNetwork/QNetworkAccessManager>\n")
    file(REMOVE_RECURSE "${poison_root}")
endif()

message(STATUS
    "Bluetooth applet pure boundary passed (${pure_source_count} files and 4 poison rejections)")

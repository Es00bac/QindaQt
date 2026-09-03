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
set(runtime_cpp_sources
    "${runtime_root}/src/bluetooth_applet_controller.h"
    "${runtime_root}/src/bluetooth_applet_controller.cpp"
    ${composition_sources})
list(LENGTH runtime_sources runtime_source_count)

set(allowed_includes
    "qindaqt/services/bluetooth_client/bluetooth_client.h"
    "qindaqt/services/bluetooth_client/qt_bluetooth_transport.h"
    "qindaqt/shell/bluetooth_applet/bluetooth_applet_types.h"
    "qindaqt/shell/bluetooth_applet/bluetooth_request_state.h"
    "qindaqt/shell/bluetooth_applet/bluetooth_applet_presentation.h"
    "qindaqt/applet_host/capability_policy_loader.h"
    "qindaqt/applet_host/host_selection.h"
    "qindaqt/applet_runtime/builtin_applet_registry.h"
    "qindaqt/applets/manifest_catalog.h"
    "QtCore/QObject"
    "QtCore/QVariantList"
    "QtCore/QVariantMap"
    "QtDBus/QDBusConnection"
    "algorithm"
    "bluetooth_applet_controller.h"
    "bluetoothappletcomposition.h"
    "memory"
    "optional")

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

# Every production C++ include is explicit. This closes bare standard/Qt
# header escapes such as filesystem, QSettings, QSaveFile, or QStandardPaths.
foreach(path IN LISTS runtime_cpp_sources)
    file(READ "${path}" content)
    string(REGEX MATCHALL "#[ \t]*include[ \t]*[<\"]([^\">]+)" includes "${content}")
    foreach(include_line IN LISTS includes)
        string(REGEX REPLACE "#[ \t]*include[ \t]*[<\"]" "" include_path "${include_line}")
        if(NOT include_path IN_LIST allowed_includes)
            list(APPEND violations "${path}: non-boundary include '${include_path}'")
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

if(violations)
    foreach(violation IN LISTS violations)
        message(SEND_ERROR "${violation}")
    endforeach()
    message(FATAL_ERROR "Bluetooth applet runtime boundary failed")
endif()

# Independent negative controls prove include-boundary and forbidden-symbol
# violations are rejected. The compiled surface test owns meta-object policy.
set(runtime_poison_count 0)
if(DEFINED POISON_ROOT AND NOT BLUETOOTH_RUNTIME_POLICY_SKIP_POISON)
    cmake_path(NORMAL_PATH POISON_ROOT OUTPUT_VARIABLE poison_root)
    file(REMOVE_RECURSE "${poison_root}")

    function(expect_runtime_poison_rejected name relative_path poison_content)
        set(case_root "${poison_root}/${name}")
        file(MAKE_DIRECTORY
             "${case_root}/src/shell"
             "${case_root}/src/shell/runtime")
        file(COPY "${runtime_root}" DESTINATION "${case_root}/src/shell")
        file(COPY ${composition_sources}
             DESTINATION "${case_root}/src/shell/runtime")
        file(APPEND "${case_root}/${relative_path}" "${poison_content}")
        execute_process(
            COMMAND "${CMAKE_COMMAND}"
                    "-DSOURCE_ROOT=${case_root}"
                    -DBLUETOOTH_RUNTIME_POLICY_SKIP_POISON=ON
                    -P "${CMAKE_CURRENT_LIST_FILE}"
            RESULT_VARIABLE poison_status
            OUTPUT_VARIABLE poison_output
            ERROR_VARIABLE poison_error)
        if(poison_status EQUAL 0)
            message(FATAL_ERROR
                "Bluetooth runtime boundary accepted ${name} poison:\n"
                "${poison_output}${poison_error}")
        endif()
        math(EXPR completed_poison_count "${runtime_poison_count} + 1")
        set(runtime_poison_count "${completed_poison_count}" PARENT_SCOPE)
    endfunction()

    set(controller_source_path
        "src/shell/bluetooth_applet/src/bluetooth_applet_controller.cpp")
    set(composition_source_path
        "src/shell/runtime/bluetoothappletcomposition.cpp")
    expect_runtime_poison_rejected(
        "service" "${controller_source_path}"
        "\n#include <qindaqt/services/bluetooth_service/resident_bluetooth_service.h>\n")
    expect_runtime_poison_rejected(
        "address-accessor" "${controller_source_path}"
        "\nQString exposedAddress = device.address;\n")
    expect_runtime_poison_rejected(
        "persistence" "${controller_source_path}"
        "\n#include <QtCore/QSettings>\n")
    expect_runtime_poison_rejected(
        "filesystem" "${composition_source_path}"
        "\n#include <QtCore/QSaveFile>\n")
    expect_runtime_poison_rejected(
        "standard-paths" "${composition_source_path}"
        "\n#include <QtCore/QStandardPaths>\n")
    file(REMOVE_RECURSE "${poison_root}")
endif()

message(STATUS
    "Bluetooth applet runtime boundary passed (${runtime_source_count} files and ${runtime_poison_count} poison rejections)")

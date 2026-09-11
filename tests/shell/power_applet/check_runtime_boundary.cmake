# SPDX-License-Identifier: GPL-3.0-or-later

if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "Missing Power applet runtime source root")
endif()

set(runtime_root "${SOURCE_ROOT}/src/shell/power_applet")
file(GLOB applet_runtime_sources
    "${runtime_root}/src/power_applet_controller.h"
    "${runtime_root}/src/power_applet_controller.cpp"
    "${runtime_root}/qml/*.qml")
set(composition_sources "")
foreach(candidate IN ITEMS
        "${SOURCE_ROOT}/src/shell/runtime/powerappletcomposition.h"
        "${SOURCE_ROOT}/src/shell/runtime/powerappletcomposition.cpp")
    if(EXISTS "${candidate}")
        list(APPEND composition_sources "${candidate}")
    endif()
endforeach()
set(runtime_sources ${applet_runtime_sources} ${composition_sources})
list(LENGTH runtime_sources runtime_source_count)
if(runtime_source_count EQUAL 0)
    message(FATAL_ERROR "Power applet runtime boundary found no sources")
endif()

set(forbidden
    "power_service"
    "PowerService"
    "UPower"
    "upower"
    "logind"
    "/sys/"
    "QProcess"
    "QFile")
set(violations "")
foreach(path IN LISTS runtime_sources)
    file(READ "${path}" content)
    foreach(token IN LISTS forbidden)
        string(FIND "${content}" "${token}" hit)
        if(NOT hit EQUAL -1)
            list(APPEND violations "${path}: forbidden token '${token}'")
        endif()
    endforeach()
endforeach()

# The production composition may construct the public QtPowerTransport on the
# session bus. That narrow root does not authorize D-Bus in the controller or
# QML renderer.
foreach(path IN LISTS applet_runtime_sources)
    file(READ "${path}" content)
    foreach(token IN ITEMS "QtDBus" "QDBus")
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
    message(FATAL_ERROR "Power applet runtime boundary failed")
endif()

set(composition_path
    "${SOURCE_ROOT}/src/shell/runtime/powerappletcomposition.cpp")
if(EXISTS "${composition_path}")
    file(READ "${composition_path}" composition_content)
    # ADR-0132: Meta+L belongs to KWin's ksmserver "Lock Session" component.
    # The shell must never register a competing global lock shortcut; the
    # applet and menu buttons keep dispatching through session_actions.
    foreach(forbidden_token IN ITEMS
            "KGlobalAccelShortcutRegistrar"
            "qindaqt_lock_session"
            "Qt::Key_L"
            "Meta | Qt::Key")
        string(FIND "${composition_content}" "${forbidden_token}" forbidden_hit)
        if(NOT forbidden_hit EQUAL -1)
            message(FATAL_ERROR
                "Power applet composition registered a global lock shortcut; Meta+L is owned by KWin's ksmserver component (ADR-0132): '${forbidden_token}'")
        endif()
    endforeach()
endif()

# Mutation-sensitive negative control: the same checker must reject a planted
# service-internal dependency. The fixture stays under the build tree.
if(DEFINED POISON_ROOT AND NOT RUNTIME_POLICY_SKIP_POISON)
    cmake_path(NORMAL_PATH POISON_ROOT OUTPUT_VARIABLE poison_root)
    file(REMOVE_RECURSE "${poison_root}")
    file(MAKE_DIRECTORY "${poison_root}/src/shell/power_applet/src")
    file(WRITE "${poison_root}/src/shell/power_applet/src/power_applet_controller.cpp"
         "#include <qindaqt/services/power_service/resident_power_service.h>\n")
    execute_process(
        COMMAND "${CMAKE_COMMAND}"
                "-DSOURCE_ROOT=${poison_root}"
                -DRUNTIME_POLICY_SKIP_POISON=ON
                -P "${CMAKE_CURRENT_LIST_FILE}"
        RESULT_VARIABLE poison_status
        OUTPUT_VARIABLE poison_output
        ERROR_VARIABLE poison_error)
    file(REMOVE_RECURSE "${poison_root}")
    if(poison_status EQUAL 0)
        message(FATAL_ERROR
            "Power applet runtime boundary accepted service-internal poison:\n"
            "${poison_output}${poison_error}")
    endif()

    # Second negative control: a reintroduced global lock shortcut must fail
    # the composition ownership check (ADR-0132).
    file(REMOVE_RECURSE "${poison_root}")
    file(MAKE_DIRECTORY "${poison_root}/src/shell/power_applet/src"
        "${poison_root}/src/shell/runtime")
    file(WRITE "${poison_root}/src/shell/power_applet/src/power_applet_controller.cpp"
         "#include <QAction>\n")
    file(WRITE "${poison_root}/src/shell/runtime/powerappletcomposition.cpp"
         "#include <QAction>\nstatic QAction *qindaqt_lock_session = nullptr;\n")
    execute_process(
        COMMAND "${CMAKE_COMMAND}"
                "-DSOURCE_ROOT=${poison_root}"
                -DRUNTIME_POLICY_SKIP_POISON=ON
                -P "${CMAKE_CURRENT_LIST_FILE}"
        RESULT_VARIABLE shortcut_poison_status
        OUTPUT_VARIABLE shortcut_poison_output
        ERROR_VARIABLE shortcut_poison_error)
    file(REMOVE_RECURSE "${poison_root}")
    if(shortcut_poison_status EQUAL 0)
        message(FATAL_ERROR
            "Power applet runtime boundary accepted a reintroduced Meta+L registration:\n"
            "${shortcut_poison_output}${shortcut_poison_error}")
    endif()
endif()

message(STATUS
    "Power applet runtime boundary passed (${runtime_source_count} files and poison rejection)")

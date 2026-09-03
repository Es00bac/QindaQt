# SPDX-License-Identifier: GPL-3.0-or-later

# Boundary probe for the task-list T2 applet (src/shell/task_list/applet).
# Standalone invocation:
#   cmake -DSOURCE_ROOT=<repo-root> -DPOISON_ROOT=<scratch-dir> \
#       -P tests/shell/task_list/check_task_list_applet_boundary.cmake
#
# AGENT-CONTRACT: the applet is a bounded presentation layer over the public
# task-list boundaries (T0 values/source, T1 authority/operations). It must
# never touch D-Bus, KWin, LayerShell, the wayland protocol, the compositor,
# service internals, the shell runtime/QML internals, the window-actions
# client, processes, files, settings, or private (_p.h) headers — even when
# such code compiles. CMake comment lines are exempt because the module's own
# CMakeLists documents this policy in prose; every code line is scanned.

if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT is required")
endif()

set(applet_directory "${SOURCE_ROOT}/src/shell/task_list/applet")
if(NOT IS_DIRECTORY "${applet_directory}")
    message(FATAL_ERROR "Task-list applet directory is missing: ${applet_directory}")
endif()

file(GLOB_RECURSE applet_files
     LIST_DIRECTORIES false
     "${applet_directory}/*.h"
     "${applet_directory}/*.cpp"
     "${applet_directory}/*.qml")
list(APPEND applet_files "${applet_directory}/CMakeLists.txt")
list(LENGTH applet_files applet_file_count)
if(applet_file_count LESS 2)
    message(FATAL_ERROR
            "Task-list applet boundary policy found no sources in ${applet_directory}")
endif()

set(forbidden_pattern
    "QDBus|<QtDBus/|KWin|LayerShell|layer-shell|QProcess|QFile|QSettings|QStandardPaths|wayland|qindaqt/compositor|qindaqt/services/|qindaqt/shell/runtime|qindaqt/shell/qml|shell_window_actions_client|qApp->|_p\\.h")

foreach(path IN LISTS applet_files)
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "Task-list applet policy input is missing: ${path}")
    endif()
    file(READ "${path}" content)
    if(path MATCHES "CMakeLists\\.txt$")
        # CMake prose documents the policy itself; only code lines are scanned.
        string(REGEX REPLACE "(^|\n)[ \t]*#[^\n]*" "\\1" content "${content}")
    endif()
    if(content MATCHES "${forbidden_pattern}")
        message(FATAL_ERROR
                "${path}: task-list applet crossed a private platform/shell boundary")
    endif()
endforeach()

if(DEFINED POISON_PROBE AND POISON_PROBE)
    message(STATUS
            "Scanned ${applet_file_count} task-list applet files (probe mode)")
    return()
endif()

if(NOT DEFINED POISON_ROOT)
    message(FATAL_ERROR "POISON_ROOT is required")
endif()

# AGENT-GUARD: poison self-test. Every forbidden token family must be rejected
# by an independent probe run against a copy of the applet tree, and the
# unmodified copy must pass — a scan that accepts everything (or rejects
# everything) is a blind gate. Each case stages its own tree so one rejection
# can never mask a blind spot in another family.
file(REMOVE_RECURSE "${POISON_ROOT}")
file(MAKE_DIRECTORY "${POISON_ROOT}")

set(poison_clean_tree "${POISON_ROOT}/clean")
file(COPY "${applet_directory}"
     DESTINATION "${poison_clean_tree}/src/shell/task_list")
execute_process(
    COMMAND
        "${CMAKE_COMMAND}"
        "-DSOURCE_ROOT=${poison_clean_tree}"
        -DPOISON_PROBE=ON
        -P "${CMAKE_CURRENT_LIST_FILE}"
    RESULT_VARIABLE clean_result
    OUTPUT_QUIET
    ERROR_QUIET
)
if(NOT clean_result EQUAL 0)
    message(FATAL_ERROR
            "Task-list applet boundary policy rejected the unmodified applet tree")
endif()

# Bodies are numbered variables, not list elements, because realistic C++
# bodies contain semicolons that would split a CMake list and silently
# scramble the staged poison.
set(poison_case_count 17)
set(poison_case_0_name "qdbus_symbol")
set(poison_case_0_file "poison.cpp")
set(poison_case_0_body "#include <QDBusConnection>\nvoid f() { QDBusConnection::sessionBus(); }\n")
set(poison_case_1_name "qtdbus_include")
set(poison_case_1_file "poison.cpp")
set(poison_case_1_body "#include <QtDBus/QDBusMessage>\nvoid f() {}\n")
set(poison_case_2_name "kwin")
set(poison_case_2_file "poison.cpp")
set(poison_case_2_body "namespace KWin { class Effect; }\nvoid f(KWin::Effect *) {}\n")
set(poison_case_3_name "layershell")
set(poison_case_3_file "poison.cpp")
set(poison_case_3_body "#include <LayerShellQt/Window>\nvoid f() {}\n")
set(poison_case_4_name "layer_shell_protocol")
set(poison_case_4_file "poison.cpp")
set(poison_case_4_body "const char *k = \"layer-shell\";\n")
set(poison_case_5_name "qprocess")
set(poison_case_5_file "poison.cpp")
set(poison_case_5_body "#include <QProcess>\nvoid f() { QProcess p; }\n")
set(poison_case_6_name "qfile")
set(poison_case_6_file "poison.cpp")
set(poison_case_6_body "#include <QFile>\nvoid f() { QFile file; }\n")
set(poison_case_7_name "qsettings")
set(poison_case_7_file "poison.cpp")
set(poison_case_7_body "#include <QSettings>\nvoid f() { QSettings s; }\n")
set(poison_case_8_name "qstandardpaths")
set(poison_case_8_file "poison.cpp")
set(poison_case_8_body "#include <QStandardPaths>\nvoid f() {}\n")
set(poison_case_9_name "wayland")
set(poison_case_9_file "poison.cpp")
set(poison_case_9_body "#include <wayland-client.h>\nvoid f() {}\n")
set(poison_case_10_name "compositor_include")
set(poison_case_10_file "poison.cpp")
set(poison_case_10_body "#include <qindaqt/compositor/compositor_state.h>\nvoid f() {}\n")
set(poison_case_11_name "services_include")
set(poison_case_11_file "poison.cpp")
set(poison_case_11_body "#include <qindaqt/services/power_client/power_client.h>\nvoid f() {}\n")
set(poison_case_12_name "shell_runtime_include")
set(poison_case_12_file "poison.cpp")
set(poison_case_12_body "#include <qindaqt/shell/runtime/main.h>\nvoid f() {}\n")
set(poison_case_13_name "shell_qml_include")
set(poison_case_13_file "poison.cpp")
set(poison_case_13_body "#include <qindaqt/shell/qml/engine.h>\nvoid f() {}\n")
set(poison_case_14_name "window_actions_client")
set(poison_case_14_file "poison.cpp")
set(poison_case_14_body "#include \"shell_window_actions_client/client.h\"\nvoid f() {}\n")
set(poison_case_15_name "qapp_macro")
set(poison_case_15_file "poison.qml")
set(poison_case_15_body "import QtQuick\nItem { Component.onCompleted: qApp->quit() }\n")
set(poison_case_16_name "private_header")
set(poison_case_16_file "poison.cpp")
set(poison_case_16_body "#include \"task_list_applet_controller_p.h\"\nvoid f() {}\n")
set(poison_case_17_name "cmakelists_kwin")
set(poison_case_17_file "CMakeLists.txt")
set(poison_case_17_body "find_package(KWin REQUIRED)\n")

foreach(poison_index RANGE ${poison_case_count})
    if(poison_index EQUAL poison_case_count)
        break()
    endif()
    set(poison_name "${poison_case_${poison_index}_name}")
    set(poison_file "${poison_case_${poison_index}_file}")
    set(poison_body "${poison_case_${poison_index}_body}")
    set(poison_tree "${POISON_ROOT}/case-${poison_name}")
    file(REMOVE_RECURSE "${poison_tree}")
    file(MAKE_DIRECTORY "${poison_tree}/src/shell")
    file(COPY "${applet_directory}"
         DESTINATION "${poison_tree}/src/shell/task_list")
    if(poison_file STREQUAL "CMakeLists.txt")
        file(APPEND
             "${poison_tree}/src/shell/task_list/applet/CMakeLists.txt"
             "\n${poison_body}")
    else()
        file(WRITE
             "${poison_tree}/src/shell/task_list/applet/${poison_file}"
             "${poison_body}")
    endif()

    execute_process(
        COMMAND
            "${CMAKE_COMMAND}"
            "-DSOURCE_ROOT=${poison_tree}"
            -DPOISON_PROBE=ON
            -P "${CMAKE_CURRENT_LIST_FILE}"
        RESULT_VARIABLE poison_result
        OUTPUT_QUIET
        ERROR_QUIET
    )
    if(poison_result EQUAL 0)
        file(REMOVE_RECURSE "${POISON_ROOT}")
        message(FATAL_ERROR
                "Task-list applet boundary policy accepted the ${poison_name} poison case")
    endif()
    file(REMOVE_RECURSE "${poison_tree}")
endforeach()
file(REMOVE_RECURSE "${POISON_ROOT}")

message(STATUS
        "Task-list applet boundary: scanned ${applet_file_count} files; clean copy accepted; ${poison_case_count} poison cases rejected")

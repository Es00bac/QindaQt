# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS
        QINDAQT_EDITOR_APP_SHELL_SOURCE_DIR
        QINDAQT_EDITOR_WINDOW_HEADER
        QINDAQT_EDITOR_WINDOW_SOURCE)
    if(NOT DEFINED ${required_variable})
        message(FATAL_ERROR "${required_variable} is required")
    endif()
endforeach()

file(GLOB_RECURSE editor_app_shell_files
     "${QINDAQT_EDITOR_APP_SHELL_SOURCE_DIR}/*.cpp"
     "${QINDAQT_EDITOR_APP_SHELL_SOURCE_DIR}/*.h")
list(LENGTH editor_app_shell_files editor_bridge_file_count)
if(editor_bridge_file_count EQUAL 0)
    message(FATAL_ERROR "Text Editor AppShell policy found no bridge/adapter files")
endif()
list(APPEND editor_app_shell_files
     "${QINDAQT_EDITOR_WINDOW_HEADER}"
     "${QINDAQT_EDITOR_WINDOW_SOURCE}")
list(REMOVE_DUPLICATES editor_app_shell_files)

list(LENGTH editor_app_shell_files editor_app_shell_file_count)

foreach(path IN LISTS editor_app_shell_files)
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "Text Editor AppShell policy input is missing: ${path}")
    endif()
    file(READ "${path}" content)

    # AGENT-GUARD: This consumer seam may depend on the public AppShell API,
    # never on the shell/compositor/service implementations that will host or
    # consume it. Adding a platform transport here would collapse the process
    # boundary documented in docs/wiki/apps/application-shell.md.
    if(content MATCHES "QDBus|LayerShell|KWin::|QProcess|QSettings|xdg-desktop-portal"
       OR content MATCHES "qindaqt/(services|service|shell|compositor|platform)/"
       OR content MATCHES "src/(services|service|shell|compositor|platform)/")
        message(FATAL_ERROR
                "${path}: Text Editor AppShell crossed a private platform/service boundary")
    endif()

    get_filename_component(file_name "${path}" NAME)
    if(content MATCHES "QFileDialog"
       AND NOT file_name MATCHES "^native_file_selection_adapter\\.(h|cpp)$")
        message(FATAL_ERROR
                "${path}: QFileDialog is allowed only in NativeFileSelectionAdapter")
    endif()
endforeach()

if(NOT QINDAQT_EDITOR_POLICY_POISON_PROBE)
    if(NOT DEFINED QINDAQT_EDITOR_POLICY_POISON_DIRECTORY)
        message(FATAL_ERROR "QINDAQT_EDITOR_POLICY_POISON_DIRECTORY is required")
    endif()

    # Exercise the exact checker in a child process. A future edit that widens
    # the allowlist enough to accept this private transport makes the positive
    # row fail instead of silently weakening the regression gate.
    set(poison_root "${QINDAQT_EDITOR_POLICY_POISON_DIRECTORY}")
    set(poison_app_shell "${poison_root}/app_shell")
    file(REMOVE_RECURSE "${poison_root}")
    file(MAKE_DIRECTORY "${poison_app_shell}" "${poison_root}/ui")
    file(WRITE "${poison_app_shell}/poisoned_bridge.cpp"
         "#include <QDBusConnection>\n")
    file(WRITE "${poison_root}/ui/editor_window.h" "#pragma once\n")
    file(WRITE "${poison_root}/ui/editor_window.cpp"
         "#include \"editor_window.h\"\n")

    execute_process(
        COMMAND
            "${CMAKE_COMMAND}"
            "-DQINDAQT_EDITOR_APP_SHELL_SOURCE_DIR=${poison_app_shell}"
            "-DQINDAQT_EDITOR_WINDOW_HEADER=${poison_root}/ui/editor_window.h"
            "-DQINDAQT_EDITOR_WINDOW_SOURCE=${poison_root}/ui/editor_window.cpp"
            -DQINDAQT_EDITOR_POLICY_POISON_PROBE=ON
            -P "${CMAKE_CURRENT_LIST_FILE}"
        RESULT_VARIABLE poison_result
        OUTPUT_QUIET
        ERROR_QUIET
    )
    file(REMOVE_RECURSE "${poison_root}")
    if(poison_result EQUAL 0)
        message(FATAL_ERROR
                "Text Editor AppShell source policy accepted a poisoned QDBus dependency")
    endif()
endif()

message(STATUS
        "Validated ${editor_app_shell_file_count} Text Editor AppShell/window files and poison rejection")

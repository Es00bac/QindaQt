# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS QINDAQT_CLIPBOARD_APPLET_SOURCE_DIR)
    if(NOT DEFINED ${required_variable})
        message(FATAL_ERROR "${required_variable} is required")
    endif()
endforeach()

file(GLOB_RECURSE applet_files
     "${QINDAQT_CLIPBOARD_APPLET_SOURCE_DIR}/*.cpp"
     "${QINDAQT_CLIPBOARD_APPLET_SOURCE_DIR}/*.h"
     "${QINDAQT_CLIPBOARD_APPLET_SOURCE_DIR}/*.qml")
list(LENGTH applet_files applet_file_count)
if(applet_file_count EQUAL 0)
    message(FATAL_ERROR "Clipboard Applet policy found no source files in ${QINDAQT_CLIPBOARD_APPLET_SOURCE_DIR}")
endif()

foreach(path IN LISTS applet_files)
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "Clipboard Applet policy input is missing: ${path}")
    endif()
    file(READ "${path}" content)

    # AGENT-GUARD: The Clipboard Applet is a bounded presentation layer.
    # It must depend only on public ClipboardModel metadata types, public Controls/Tokens,
    # and the injected client seam. Any direct access to Wayland protocol headers,
    # DBus, host clipboards, compositor internals, or raw system files is strictly forbidden.
    if(content MATCHES "QDBus|LayerShell|KWin::|QProcess|sysfs|wayland-client|wayland-server"
       OR content MATCHES "qindaqt/(compositor|platform)/"
       OR content MATCHES "src/(compositor|platform)/")
        message(FATAL_ERROR
                "${path}: Clipboard Applet crossed a private platform/compositor boundary")
    endif()
endforeach()

if(NOT QINDAQT_CLIPBOARD_APPLET_POISON_PROBE)
    if(NOT DEFINED QINDAQT_CLIPBOARD_APPLET_POISON_DIRECTORY)
        message(FATAL_ERROR "QINDAQT_CLIPBOARD_APPLET_POISON_DIRECTORY is required")
    endif()

    set(poison_root "${QINDAQT_CLIPBOARD_APPLET_POISON_DIRECTORY}")
    file(REMOVE_RECURSE "${poison_root}")
    file(MAKE_DIRECTORY "${poison_root}")
    file(WRITE "${poison_root}/poisoned_file.cpp"
         "#include <QDBusConnection>\n")

    execute_process(
        COMMAND
            "${CMAKE_COMMAND}"
            "-DQINDAQT_CLIPBOARD_APPLET_SOURCE_DIR=${poison_root}"
            -DQINDAQT_CLIPBOARD_APPLET_POISON_PROBE=ON
            -P "${CMAKE_CURRENT_LIST_FILE}"
        RESULT_VARIABLE poison_result
        OUTPUT_QUIET
        ERROR_QUIET
    )
    file(REMOVE_RECURSE "${poison_root}")
    if(poison_result EQUAL 0)
        message(FATAL_ERROR
                "Clipboard Applet boundary policy accepted a poisoned QDBus dependency")
    endif()
endif()

message(STATUS
        "Validated ${applet_file_count} Clipboard Applet source/QML files and poison probe rejection")

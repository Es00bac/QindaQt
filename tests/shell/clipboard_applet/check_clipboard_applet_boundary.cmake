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
    # DBus, host clipboards, compositor internals, or raw system files is strictly
    # refused. The host-clipboard patterns are load-bearing: reading or including
    # QtGui/QClipboard, QGuiApplication::clipboard, or an external clipboard helper
    # bypasses the authenticated client seam and re-discloses content behind the
    # lock/privacy fence. Case matters — lowercase "clipboard" is vocabulary, the
    # capitalized Qt/GTK symbols are forbidden platform access.
    if(content MATCHES "QDBus|LayerShell|KWin::|QProcess|sysfs|wayland-client|wayland-server"
       OR content MATCHES "QClipboard|QGuiApplication|qt_clipboard|gtk_clipboard|GtkClipboard"
       OR content MATCHES "xclip|xsel|wl-copy|wl-paste|X11/Xlib|XGetSelection|XStoreBytes"
       OR content MATCHES "qindaqt/(compositor|platform)/"
       OR content MATCHES "src/(compositor|platform)/")
        message(FATAL_ERROR
                "${path}: Clipboard Applet crossed a private platform/host-clipboard boundary")
    endif()
endforeach()

if(NOT QINDAQT_CLIPBOARD_APPLET_POISON_PROBE)
    if(NOT DEFINED QINDAQT_CLIPBOARD_APPLET_POISON_DIRECTORY)
        message(FATAL_ERROR "QINDAQT_CLIPBOARD_APPLET_POISON_DIRECTORY is required")
    endif()

    set(poison_root "${QINDAQT_CLIPBOARD_APPLET_POISON_DIRECTORY}")
    file(REMOVE_RECURSE "${poison_root}")
    file(MAKE_DIRECTORY "${poison_root}")

    # AGENT-GUARD: every poison case below must be rejected by an independent
    # probe run. Staging them one at a time proves each pattern family fails
    # on its own; a shared directory would let one rejection mask a blind spot
    # in another pattern. Bodies are numbered variables, not list elements,
    # because realistic C++ bodies contain semicolons that would split a
    # CMake list and silently scramble the staged poison.
    set(poison_case_count 4)
    set(poison_case_0_name "qdbus")
    set(poison_case_0_body "#include <QDBusConnection>\nvoid f() { QDBusConnection::sessionBus(); }\n")
    set(poison_case_1_name "host_clipboard_include")
    set(poison_case_1_body "#include <QtGui/QClipboard>\nvoid f() {}\n")
    set(poison_case_2_name "host_clipboard_read")
    set(poison_case_2_body "#include <QGuiApplication>\n#include <QtGui/QClipboard>\nQString f() { return QGuiApplication::clipboard()->text(); }\n")
    set(poison_case_3_name "external_helper")
    set(poison_case_3_body "#include <QProcess>\nvoid f() { QProcess::execute(\"xclip\"); }\n")

    foreach(poison_index RANGE ${poison_case_count})
        if(poison_index EQUAL poison_case_count)
            break()
        endif()
        set(poison_name "${poison_case_${poison_index}_name}")
        set(poison_body "${poison_case_${poison_index}_body}")
        file(REMOVE_RECURSE "${poison_root}")
        file(MAKE_DIRECTORY "${poison_root}")
        file(WRITE "${poison_root}/${poison_name}_poisoned_file.cpp" "${poison_body}")

        execute_process(
            COMMAND
                "${CMAKE_COMMAND}"
                "-DQINDAQT_CLIPBOARD_APPLET_SOURCE_DIR=${poison_root}"
                "-DQINDAQT_CLIPBOARD_APPLET_POISON_PROBE=ON"
                -P "${CMAKE_CURRENT_LIST_FILE}"
            RESULT_VARIABLE poison_result
            OUTPUT_QUIET
            ERROR_QUIET
        )
        if(poison_result EQUAL 0)
            file(REMOVE_RECURSE "${poison_root}")
            message(FATAL_ERROR
                    "Clipboard Applet boundary policy accepted the ${poison_name} poison case")
        endif()
    endforeach()
    file(REMOVE_RECURSE "${poison_root}")
endif()

message(STATUS
        "Validated ${applet_file_count} Clipboard Applet source/QML files and poison probe rejection")

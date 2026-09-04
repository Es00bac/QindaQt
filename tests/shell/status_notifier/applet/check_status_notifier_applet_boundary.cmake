# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS QINDAQT_STATUS_NOTIFIER_APPLET_SOURCE_DIR)
    if(NOT DEFINED ${required_variable})
        message(FATAL_ERROR "${required_variable} is required")
    endif()
endforeach()

file(GLOB_RECURSE applet_files
     "${QINDAQT_STATUS_NOTIFIER_APPLET_SOURCE_DIR}/*.cpp"
     "${QINDAQT_STATUS_NOTIFIER_APPLET_SOURCE_DIR}/*.h"
     "${QINDAQT_STATUS_NOTIFIER_APPLET_SOURCE_DIR}/*.qml")
list(LENGTH applet_files applet_file_count)
if(applet_file_count EQUAL 0)
    message(FATAL_ERROR "Status Notifier Applet policy found no source files in ${QINDAQT_STATUS_NOTIFIER_APPLET_SOURCE_DIR}")
endif()

foreach(path IN LISTS applet_files)
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "Status Notifier Applet policy input is missing: ${path}")
    endif()
    file(READ "${path}" content)

    # AGENT-GUARD: The Status Notifier Applet is a bounded presentation layer.
    # All D-Bus wire traffic belongs to the S1 transports (watcher service,
    # item monitor); the adapter may only hold the injected QDBusConnection
    # type — it must never open a connection, create interface proxies, watch
    # services, or issue pending calls itself. Process spawning, Wayland/KWin/
    # LayerShell reach-through, private headers, and sibling-module src/
    # includes are refused for the same reason the clipboard lane refuses
    # them: they bypass the authenticated seam.
    if(content MATCHES "QDBusInterface|QDBusAbstractInterface|QDBusPendingCall|QDBusServiceWatcher"
       OR content MATCHES "sessionBus\\(|systemBus\\("
       OR content MATCHES "QProcess"
       OR content MATCHES "LayerShell|KWin::|wayland-client|wayland-server"
       OR content MATCHES "_p\\.h"
       OR content MATCHES "qindaqt/(compositor|platform)/"
       OR content MATCHES "src/(compositor|platform)/")
        message(FATAL_ERROR
                "${path}: Status Notifier Applet crossed a private transport/platform boundary")
    endif()
endforeach()

if(NOT QINDAQT_STATUS_NOTIFIER_APPLET_POISON_PROBE)
    if(NOT DEFINED QINDAQT_STATUS_NOTIFIER_APPLET_POISON_DIRECTORY)
        message(FATAL_ERROR "QINDAQT_STATUS_NOTIFIER_APPLET_POISON_DIRECTORY is required")
    endif()

    set(poison_root "${QINDAQT_STATUS_NOTIFIER_APPLET_POISON_DIRECTORY}")
    file(REMOVE_RECURSE "${poison_root}")
    file(MAKE_DIRECTORY "${poison_root}")

    # AGENT-GUARD: every poison case below must be rejected by an independent
    # probe run. Staging them one at a time proves each pattern family fails
    # on its own; a shared directory would let one rejection mask a blind spot
    # in another pattern. Bodies are numbered variables, not list elements,
    # because realistic C++ bodies contain semicolons that would split a
    # CMake list and silently scramble the staged poison.
    set(poison_case_count 8)
    set(poison_case_0_name "qdbus_interface")
    set(poison_case_0_body "#include <QtDBus/QDBusInterface>\nvoid f() { QDBusInterface i(QStringLiteral(\"org.x\"), QStringLiteral(\"/\")); }\n")
    set(poison_case_1_name "session_bus")
    set(poison_case_1_body "#include <QtDBus/QDBusConnection>\nvoid f() { auto c = QDBusConnection::sessionBus(); }\n")
    set(poison_case_2_name "system_bus")
    set(poison_case_2_body "#include <QtDBus/QDBusConnection>\nvoid f() { auto c = QDBusConnection::systemBus(); }\n")
    set(poison_case_3_name "service_watcher")
    set(poison_case_3_body "#include <QtDBus/QDBusServiceWatcher>\nvoid f() { QDBusServiceWatcher w; }\n")
    set(poison_case_4_name "pending_call")
    set(poison_case_4_body "#include <QtDBus/QDBusPendingCall>\nvoid f() { QDBusPendingCall c = {}; }\n")
    set(poison_case_5_name "process_spawn")
    set(poison_case_5_body "#include <QProcess>\nvoid f() { QProcess::execute(QStringLiteral(\"dbus-send\")); }\n")
    set(poison_case_6_name "private_header")
    set(poison_case_6_body "#include <qindaqt/shell/status_notifier/status_notifier_registry_p.h>\nvoid f() {}\n")
    set(poison_case_7_name "wayland")
    set(poison_case_7_body "#include <wayland-client.h>\nvoid f() {}\n")

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
                "-DQINDAQT_STATUS_NOTIFIER_APPLET_SOURCE_DIR=${poison_root}"
                "-DQINDAQT_STATUS_NOTIFIER_APPLET_POISON_PROBE=ON"
                -P "${CMAKE_CURRENT_LIST_FILE}"
            RESULT_VARIABLE poison_result
            OUTPUT_QUIET
            ERROR_QUIET
        )
        if(poison_result EQUAL 0)
            file(REMOVE_RECURSE "${poison_root}")
            message(FATAL_ERROR
                    "Status Notifier Applet boundary policy accepted the ${poison_name} poison case")
        endif()
    endforeach()
    file(REMOVE_RECURSE "${poison_root}")
endif()

message(STATUS
        "Validated ${applet_file_count} Status Notifier Applet source/QML files and poison probe rejection")

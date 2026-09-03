# SPDX-License-Identifier: GPL-3.0-or-later

# Launcher L1 source-policy gate: the pure L0 model stays free of platform
# tokens, and the runtime's platform reach is confined to its dedicated
# adapter files. A poison negative control proves the checker bites.

if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "Missing launcher source root")
endif()

set(module_root "${SOURCE_ROOT}/src/shell/launcher")
file(GLOB pure_sources
    "${module_root}/src/application_catalog.cpp"
    "${module_root}/src/desktop_entry_parser.cpp"
    "${module_root}/src/launcher_category_model.cpp"
    "${module_root}/src/launcher_pinned_recent.cpp"
    "${module_root}/src/launcher_presentation.cpp"
    "${module_root}/src/launcher_search_ranker.cpp"
    "${module_root}/include/qindaqt/shell_launcher/*.h")
file(GLOB scanner_sources "${module_root}/src/application_scanner.*")
file(GLOB spawner_sources "${module_root}/src/launch_spawner.*")
file(GLOB activator_sources "${module_root}/src/launch_activator.*")
file(GLOB controller_sources
    "${module_root}/src/launcher_applet_controller.*"
    "${module_root}/src/launcher_persistence.*")
file(GLOB planning_sources
    "${module_root}/src/launch_execution.*"
    "${module_root}/src/launch_executor.*")
file(GLOB applet_qml "${module_root}/qml/*.qml")

list(LENGTH pure_sources pure_count)
if(pure_count EQUAL 0 AND NOT LAUNCHER_POLICY_SKIP_POISON)
    message(FATAL_ERROR "Launcher boundary found no pure sources")
endif()

set(violations "")

function(launcher_forbid files description)
    foreach(path IN LISTS ${files})
        file(READ "${path}" content)
        foreach(token IN LISTS ARGN)
            string(FIND "${content}" "${token}" hit)
            if(NOT hit EQUAL -1)
                set(violations ${violations}
                    "${path}: forbidden token '${token}' (${description})"
                    PARENT_SCOPE)
            endif()
        endforeach()
    endforeach()
endfunction()

# The pure L0 model: no filesystem, process, bus, QML, or environment reach.
# ("QML" the word appears in prose comments; the include/class tokens are the
# enforceable boundary.)
launcher_forbid(pure_sources "pure L0 model"
    "QFile" "QDir" "QFileSystemWatcher" "QProcess" "QDBus" "QtDBus"
    "QQml" "QQuick" "qgetenv" "std::getenv" "settings_client")

# Platform reach is confined to its adapter file.
launcher_forbid(controller_sources "controller/persistence"
    "QProcess" "QFileSystemWatcher" "QDBus" "QtDBus" "QFile" "QDir"
    "qgetenv" "std::getenv")
launcher_forbid(scanner_sources "scanner" "QProcess" "QDBus" "QtDBus" "qgetenv")
launcher_forbid(spawner_sources "spawner" "QFileSystemWatcher" "QDBus" "QtDBus")
launcher_forbid(activator_sources "activator" "QProcess" "QFileSystemWatcher")
launcher_forbid(planning_sources "execution planning"
    "QFile" "QDir" "QFileSystemWatcher" "QProcess" "QDBus" "QtDBus"
    "qgetenv" "std::getenv")
launcher_forbid(applet_qml "compiled QML"
    "QProcess" "QDBus" "SettingsClient" "settings_client")

if(violations)
    foreach(violation IN LISTS violations)
        message(SEND_ERROR "${violation}")
    endforeach()
    message(FATAL_ERROR "Launcher boundary failed")
endif()

# Mutation-sensitive negative control: the same checker must reject a planted
# D-Bus dependency in the controller.
if(DEFINED POISON_ROOT AND NOT LAUNCHER_POLICY_SKIP_POISON)
    cmake_path(NORMAL_PATH POISON_ROOT OUTPUT_VARIABLE poison_root)
    file(REMOVE_RECURSE "${poison_root}")
    file(MAKE_DIRECTORY "${poison_root}/src/shell/launcher/src")
    file(WRITE "${poison_root}/src/shell/launcher/src/launcher_applet_controller.cpp"
         "#include <QDBusConnection>\n")
    execute_process(
        COMMAND "${CMAKE_COMMAND}"
                "-DSOURCE_ROOT=${poison_root}"
                -DLAUNCHER_POLICY_SKIP_POISON=ON
                -P "${CMAKE_CURRENT_LIST_FILE}"
        RESULT_VARIABLE poison_status
        OUTPUT_VARIABLE poison_output
        ERROR_VARIABLE poison_error)
    file(REMOVE_RECURSE "${poison_root}")
    if(poison_status EQUAL 0)
        message(FATAL_ERROR
            "Launcher boundary accepted controller D-Bus poison:\n"
            "${poison_output}${poison_error}")
    endif()
endif()

message(STATUS "Launcher boundary passed")

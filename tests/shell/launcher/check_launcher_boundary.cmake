# SPDX-License-Identifier: GPL-3.0-or-later

# Launcher L1 source-policy gate: the pure L0 model stays free of platform
# tokens, and the runtime's platform reach is confined to its dedicated
# adapter files. A poison negative control proves the checker bites.

if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "Missing launcher source root")
endif()

set(module_root "${SOURCE_ROOT}/src/shell/launcher")
set(test_root "${SOURCE_ROOT}/tests/shell/launcher")
set(composition_root "${SOURCE_ROOT}/src/shell/runtime")
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
file(GLOB composition_sources
    "${composition_root}/launcherappletcomposition.h"
    "${composition_root}/launcherappletcomposition.cpp")

list(LENGTH pure_sources pure_count)
if(pure_count EQUAL 0 AND NOT LAUNCHER_POLICY_SKIP_POISON)
    message(FATAL_ERROR "Launcher boundary found no pure sources")
endif()

set(violations "")

if(NOT LAUNCHER_POLICY_SKIP_POISON)
    # AGENT-GUARD: Headless adapter consumers must never regain a transitive
    # GUI platform dependency. QTEST_MAIN changes construction based on link
    # defines, so enforce both the target boundary and explicit guiless mains.
    file(READ "${module_root}/CMakeLists.txt" launcher_cmake)
    string(REGEX MATCH
        "target_link_libraries\\([ \t\r\n]*qindaqt_shell_launcher_runtime[^\\)]*\\)"
        runtime_link_block "${launcher_cmake}")
    if(runtime_link_block STREQUAL "")
        list(APPEND violations
            "${module_root}/CMakeLists.txt: runtime link interface was not found")
    else()
        foreach(token IN ITEMS Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2)
            string(FIND "${runtime_link_block}" "${token}" hit)
            if(NOT hit EQUAL -1)
                list(APPEND violations
                    "${module_root}/CMakeLists.txt: runtime leaks '${token}' to headless consumers")
            endif()
        endforeach()
    endif()

    foreach(test_source IN ITEMS
            tst_application_scanner.cpp
            tst_launch_executor.cpp
            tst_launcher_persistence.cpp
            tst_launcher_controller.cpp)
        file(READ "${test_root}/${test_source}" test_contents)
        string(FIND "${test_contents}" "QTEST_GUILESS_MAIN" guiless_main)
        if(guiless_main EQUAL -1)
            list(APPEND violations
                "${test_root}/${test_source}: adapter test must use QTEST_GUILESS_MAIN")
        endif()
    endforeach()
endif()

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
launcher_forbid(composition_sources "production composition"
    "settings_service" "SettingsService" "services/settings_service"
    "QQml" "QQuick" "std::getenv" "qgetenv")

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

if(DEFINED POISON_ROOT AND NOT LAUNCHER_POLICY_SKIP_POISON)
    cmake_path(NORMAL_PATH POISON_ROOT OUTPUT_VARIABLE poison_root)
    set(composition_poison "${poison_root}-composition")
    file(REMOVE_RECURSE "${composition_poison}")
    file(MAKE_DIRECTORY "${composition_poison}/src/shell/runtime")
    file(WRITE
         "${composition_poison}/src/shell/runtime/launcherappletcomposition.cpp"
         "#include <qindaqt/services/settings_service/settings_service.h>\n")
    execute_process(
        COMMAND "${CMAKE_COMMAND}"
                "-DSOURCE_ROOT=${composition_poison}"
                -DLAUNCHER_POLICY_SKIP_POISON=ON
                -P "${CMAKE_CURRENT_LIST_FILE}"
        RESULT_VARIABLE composition_poison_status
        OUTPUT_VARIABLE composition_poison_output
        ERROR_VARIABLE composition_poison_error)
    file(REMOVE_RECURSE "${composition_poison}")
    if(composition_poison_status EQUAL 0)
        message(FATAL_ERROR
            "Launcher boundary accepted service-internal composition poison:\n"
            "${composition_poison_output}${composition_poison_error}")
    endif()
endif()

message(STATUS "Launcher boundary passed")

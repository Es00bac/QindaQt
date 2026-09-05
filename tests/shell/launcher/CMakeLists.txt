# SPDX-License-Identifier: GPL-3.0-or-later

# Launcher tests. The block below lets this directory configure and run on its
# own for a focused lane build; the repository build wires it from
# tests/CMakeLists.txt.

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.25)
    project(QindaQtShellLauncherTests LANGUAGES CXX)

    include(CTest)
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake")
    include(QindaQtCompiler)
    find_package(ECM 6.0 REQUIRED NO_MODULE)
    list(APPEND CMAKE_MODULE_PATH "${ECM_MODULE_PATH}")
    include(KDEInstallDirs6)
    find_package(Qt6 6.11 REQUIRED COMPONENTS
                 Core DBus Gui Qml Quick QuickControls2 Svg Test)
    qt_standard_project_setup(REQUIRES 6.11)

    # The L1 runtime adapters consume these public boundaries; add them
    # before the launcher module so its runtime registers immediately.
    add_subdirectory("../../../src/core" "${CMAKE_BINARY_DIR}/src/core")
    add_subdirectory("../../../src/profiles" "${CMAKE_BINARY_DIR}/src/profiles")
    add_subdirectory("../../../src/themes" "${CMAKE_BINARY_DIR}/src/themes")
    add_subdirectory("../../../src/design_tokens"
                     "${CMAKE_BINARY_DIR}/src/design_tokens")
    add_subdirectory("../../../src/controls" "${CMAKE_BINARY_DIR}/src/controls")
    add_subdirectory("../../../src/services/settings_protocol"
                     "${CMAKE_BINARY_DIR}/src/services/settings_protocol")
    add_subdirectory("../../../src/services/settings_client"
                     "${CMAKE_BINARY_DIR}/src/services/settings_client")
    add_subdirectory("../../../src/applets" "${CMAKE_BINARY_DIR}/src/applets")
    add_subdirectory("../../../src/applet_host"
                     "${CMAKE_BINARY_DIR}/src/applet_host")
    add_subdirectory("../../../src/applet_runtime"
                     "${CMAKE_BINARY_DIR}/src/applet_runtime")
    set(QINDAQT_LAUNCHER_DEPENDENCIES_READY ON)
    set(QINDAQT_LAUNCHER_PURE_ONLY ON)
    add_subdirectory(
        "../../../src/shell/launcher"
        "${CMAKE_BINARY_DIR}/src/shell/launcher"
    )
    unset(QINDAQT_LAUNCHER_PURE_ONLY)
    add_subdirectory(
        "../../../src/shell/icons"
        "${CMAKE_BINARY_DIR}/src/shell/icons"
    )
    qindaqt_add_launcher_runtime()
    set(launcher_source_root "${CMAKE_CURRENT_SOURCE_DIR}/../../..")
else()
    find_package(Qt6 6.11 REQUIRED COMPONENTS Svg)
    set(launcher_source_root "${CMAKE_SOURCE_DIR}")
endif()

function(qindaqt_add_shell_launcher_test target source test_name)
    qt_add_executable(${target} ${source})
    target_link_libraries(${target} PRIVATE QindaQt::ShellLauncher Qt6::Test)
    qindaqt_enable_warnings(${target})
    add_test(NAME ${test_name} COMMAND ${target})
    set_tests_properties(
        ${test_name}
        PROPERTIES ENVIRONMENT_MODIFICATION
            "DBUS_SESSION_BUS_ADDRESS=unset:;DISPLAY=unset:;WAYLAND_DISPLAY=unset:;XDG_RUNTIME_DIR=set:${CMAKE_CURRENT_BINARY_DIR}/isolated-runtime"
    )
endfunction()

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/isolated-runtime")
file(CHMOD "${CMAKE_CURRENT_BINARY_DIR}/isolated-runtime"
     PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)

qindaqt_add_shell_launcher_test(
    qindaqt_desktop_entry_parser_tests
    tst_desktop_entry_parser.cpp
    qindaqt.launcher-desktop-entry-parser
)
qindaqt_add_shell_launcher_test(
    qindaqt_application_catalog_tests
    tst_application_catalog.cpp
    qindaqt.launcher-application-catalog
)
qindaqt_add_shell_launcher_test(
    qindaqt_launcher_category_model_tests
    tst_launcher_category_model.cpp
    qindaqt.launcher-category-model
)
qindaqt_add_shell_launcher_test(
    qindaqt_launcher_search_ranker_tests
    tst_launcher_search_ranker.cpp
    qindaqt.launcher-search-ranker
)
qindaqt_add_shell_launcher_test(
    qindaqt_launcher_pinned_recent_tests
    tst_launcher_pinned_recent.cpp
    qindaqt.launcher-pinned-recent
)
qindaqt_add_shell_launcher_test(
    qindaqt_launcher_presentation_tests
    tst_launcher_presentation.cpp
    qindaqt.launcher-presentation
)

# L1 adapter and applet tests; they exist only where the runtime target does.
if(TARGET qindaqt_shell_launcher_runtime)
    find_package(Qt6 6.11 REQUIRED COMPONENTS QuickTest)
    get_target_property(launcher_runtime_links
                        qindaqt_shell_launcher_runtime INTERFACE_LINK_LIBRARIES)
    foreach(gui_library IN ITEMS Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2)
        if(gui_library IN_LIST launcher_runtime_links)
            message(FATAL_ERROR
                "Launcher runtime leaks ${gui_library} to headless adapter consumers")
        endif()
    endforeach()

    function(qindaqt_add_launcher_runtime_test target source test_name)
        qt_add_executable(${target} ${source})
        target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
        target_link_libraries(
            ${target}
            PRIVATE QindaQt::ShellLauncherRuntime Qt6::Test
        )
        qindaqt_enable_warnings(${target})
        add_test(NAME ${test_name} COMMAND ${target})
        set_tests_properties(
            ${test_name}
            PROPERTIES
                LABELS "shell;launcher;runtime"
                ENVIRONMENT_MODIFICATION
                    "DBUS_SESSION_BUS_ADDRESS=unset:;DISPLAY=unset:;WAYLAND_DISPLAY=unset:;XDG_RUNTIME_DIR=set:${CMAKE_CURRENT_BINARY_DIR}/isolated-runtime"
        )
    endfunction()

    qindaqt_add_launcher_runtime_test(
        qindaqt_launcher_scanner_tests
        tst_application_scanner.cpp
        qindaqt.launcher-application-scanner
    )

    if(TARGET qindaqt-shell)
        find_program(DBUS_RUN_SESSION_EXECUTABLE dbus-run-session REQUIRED)
        file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/composition-scratch")
        qt_add_executable(
            qindaqt_launcher_composition_tests
            tst_launcher_composition.cpp
            "${launcher_source_root}/src/shell/runtime/launcherappletcomposition.cpp"
            "${launcher_source_root}/src/shell/runtime/launcherappletcomposition.h"
        )
        target_include_directories(
            qindaqt_launcher_composition_tests
            PRIVATE
                "${CMAKE_CURRENT_SOURCE_DIR}"
                "${launcher_source_root}/src/shell/runtime"
        )
        target_link_libraries(
            qindaqt_launcher_composition_tests
            PRIVATE
                QindaQt::AppletHost
                QindaQt::AppletRuntime
                QindaQt::SettingsClient
                QindaQt::ShellLauncherRuntime
                Qt6::DBus
                Qt6::Test
        )
        target_compile_definitions(
            qindaqt_launcher_composition_tests
            PRIVATE
                QINDAQT_SOURCE_DIR="${launcher_source_root}"
                QINDAQT_TEST_SCRATCH="${CMAKE_CURRENT_BINARY_DIR}/composition-scratch"
        )
        qindaqt_enable_warnings(qindaqt_launcher_composition_tests)
        add_test(
            NAME qindaqt.launcher-composition
            COMMAND "${DBUS_RUN_SESSION_EXECUTABLE}" --
                    $<TARGET_FILE:qindaqt_launcher_composition_tests>
        )
        set_tests_properties(
            qindaqt.launcher-composition
            PROPERTIES
                LABELS "shell;launcher;runtime;private-bus;hostile"
                ENVIRONMENT_MODIFICATION
                    "DBUS_SESSION_BUS_ADDRESS=unset:;DISPLAY=unset:;WAYLAND_DISPLAY=unset:;XDG_RUNTIME_DIR=set:${CMAKE_CURRENT_BINARY_DIR}/isolated-runtime;TMPDIR=set:${CMAKE_CURRENT_BINARY_DIR}/composition-scratch"
        )
    endif()

    add_test(
        NAME qindaqt.launcher-panel-dispatcher
        COMMAND
            $<TARGET_FILE:Qt6::qmltestrunner>
            -import "${launcher_source_root}/tests/shell/qml/imports"
            -input "${CMAKE_CURRENT_SOURCE_DIR}/tst_launcher_dispatcher.qml"
    )
    set_tests_properties(
        qindaqt.launcher-panel-dispatcher
        PROPERTIES
            LABELS "shell;launcher;runtime;offscreen;accessibility"
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_FATAL_WARNINGS=1"
            ENVIRONMENT_MODIFICATION
                "DBUS_SESSION_BUS_ADDRESS=unset:;DISPLAY=unset:;WAYLAND_DISPLAY=unset:;XDG_RUNTIME_DIR=set:${CMAKE_CURRENT_BINARY_DIR}/isolated-runtime"
    )
    set_tests_properties(qindaqt.launcher-application-scanner PROPERTIES TIMEOUT 10)
    qindaqt_add_launcher_runtime_test(
        qindaqt_launcher_execution_tests
        tst_launch_execution.cpp
        qindaqt.launcher-execution
    )
    qindaqt_add_launcher_runtime_test(
        qindaqt_launcher_executor_tests
        tst_launch_executor.cpp
        qindaqt.launcher-executor
    )
    qindaqt_add_launcher_runtime_test(
        qindaqt_launcher_persistence_tests
        tst_launcher_persistence.cpp
        qindaqt.launcher-persistence
    )
    if(TARGET QindaQt::SettingsService)
        find_program(LAUNCHER_DBUS_DAEMON dbus-daemon REQUIRED)
        qindaqt_add_launcher_runtime_test(
            qindaqt_launcher_settings_contract_tests
            tst_launcher_settings_contract.cpp
            qindaqt.launcher-settings-contract
        )
        target_link_libraries(qindaqt_launcher_settings_contract_tests
            PRIVATE QindaQt::SettingsService Qt6::DBus)
        target_compile_definitions(qindaqt_launcher_settings_contract_tests PRIVATE
            QINDAQT_SOURCE_DIR="${launcher_source_root}"
            QINDAQT_DBUS_DAEMON="${LAUNCHER_DBUS_DAEMON}")
    endif()
    qindaqt_add_launcher_runtime_test(
        qindaqt_launcher_controller_tests
        tst_launcher_controller.cpp
        qindaqt.launcher-controller
    )

    qt_add_executable(qindaqt_launcher_qml_tests tst_launcher_qml.cpp)
    target_include_directories(qindaqt_launcher_qml_tests
                               PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(
        qindaqt_launcher_qml_tests
        PRIVATE
            QindaQt::ShellLauncherRuntime
            QindaQt::TokensQml
            QindaQt::ShellIcons
            qindaqt_shell_iconsplugin
            qindaqt_themes
            Qt6::Gui
            Qt6::Qml
            Qt6::Quick
            Qt6::Test
    )
    target_compile_definitions(
        qindaqt_launcher_qml_tests
        PRIVATE
            QINDAQT_LAUNCHER_QML_IMPORT_PATH="${CMAKE_BINARY_DIR}/qml"
            QINDAQT_APPLET_ICON_FIXTURE_ROOT="${CMAKE_CURRENT_BINARY_DIR}/icon-resolution"
            QINDAQT_SOURCE_DIR="${launcher_source_root}"
    )
    qindaqt_enable_warnings(qindaqt_launcher_qml_tests)
    qt_import_qml_plugins(qindaqt_launcher_qml_tests)
    # AGENT-CONTRACT: The applet imports QindaQt.Tokens/Controls/Shell.Launcher
    # through the build-tree QML directories, so the module plugins must exist
    # before the offscreen scene loads.
    add_dependencies(
        qindaqt_launcher_qml_tests
        qindaqt_shell_launcher_qmlplugin
        qindaqt_tokens_qmlplugin
        qindaqt_controls_qmlplugin
    )
    add_test(NAME qindaqt.launcher-offscreen COMMAND qindaqt_launcher_qml_tests)
    set_tests_properties(
        qindaqt.launcher-offscreen
        PROPERTIES
            LABELS "shell;launcher;runtime;offscreen;accessibility"
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_LINUX_ACCESSIBILITY_ALWAYS_ON=1;QT_FATAL_WARNINGS=1"
            ENVIRONMENT_MODIFICATION
                "DBUS_SESSION_BUS_ADDRESS=unset:;DISPLAY=unset:;WAYLAND_DISPLAY=unset:;XDG_RUNTIME_DIR=set:${CMAKE_CURRENT_BINARY_DIR}/isolated-runtime"
    )

    add_test(
        NAME qindaqt.launcher-runtime-boundary
        COMMAND
            "${CMAKE_COMMAND}"
            "-DSOURCE_ROOT=${launcher_source_root}"
            "-DPOISON_ROOT=${CMAKE_CURRENT_BINARY_DIR}/launcher-boundary-poison"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/check_launcher_boundary.cmake"
    )
    set_tests_properties(
        qindaqt.launcher-runtime-boundary
        PROPERTIES
            LABELS "shell;launcher;runtime;policy;hostile"
            ENVIRONMENT_MODIFICATION
                "DBUS_SESSION_BUS_ADDRESS=unset:;DISPLAY=unset:;WAYLAND_DISPLAY=unset:;XDG_RUNTIME_DIR=set:${CMAKE_CURRENT_BINARY_DIR}/isolated-runtime"
    )

    add_test(
        NAME qindaqt.launcher-contract-text
        COMMAND
            "${CMAKE_COMMAND}"
            "-DSOURCE_ROOT=${launcher_source_root}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/check_launcher_contract_text.cmake"
    )
    set_tests_properties(
        qindaqt.launcher-contract-text
        PROPERTIES
            LABELS "shell;launcher;documentation;policy;hostile"
            ENVIRONMENT_MODIFICATION
                "DBUS_SESSION_BUS_ADDRESS=unset:;DISPLAY=unset:;WAYLAND_DISPLAY=unset:;XDG_RUNTIME_DIR=set:${CMAKE_CURRENT_BINARY_DIR}/isolated-runtime"
    )

    if(TARGET qindaqt_applet_runtime AND TARGET qindaqt-shell)
        find_package(KF6GlobalAccel 6.0 REQUIRED CONFIG)
        qt_add_executable(qindaqt_launcher_installed_probe
                          tst_launcher_installed_probe.cpp)
        target_link_libraries(
            qindaqt_launcher_installed_probe
            PRIVATE QindaQt::AppletRuntime QindaQt::DesignTokens
                    QindaQt::ShellIcons qindaqt_themes QindaQt::TokensQml
                    qindaqt_shell_iconsplugin
                    Qt6::Gui Qt6::Qml Qt6::Quick
        )
        qt_import_qml_plugins(qindaqt_launcher_installed_probe)
        qindaqt_enable_warnings(qindaqt_launcher_installed_probe)

        add_test(
            NAME qindaqt.launcher-installed-package
            COMMAND
                "${CMAKE_COMMAND}"
                "-DQINDAQT_CMAKE=${CMAKE_COMMAND}"
                "-DQINDAQT_BUILD_ROOT=${CMAKE_BINARY_DIR}"
                "-DQINDAQT_STAGE=${CMAKE_CURRENT_BINARY_DIR}/installed-launcher"
                "-DQINDAQT_PROBE=$<TARGET_FILE:qindaqt_launcher_installed_probe>"
                "-DQINDAQT_INSTALL_BINDIR=${CMAKE_INSTALL_BINDIR}"
                "-DQINDAQT_INSTALL_QMLDIR=${QT6_INSTALL_QML}"
                "-DQINDAQT_INSTALL_DATADIR=${CMAKE_INSTALL_DATADIR}"
                "-DQINDAQT_INSTALL_LIBDIR=${CMAKE_INSTALL_LIBDIR}"
                "-DQINDAQT_KF6_GLOBALACCEL_LIBRARY=$<TARGET_FILE:KF6::GlobalAccel>"
                "-DQINDAQT_KF6_GLOBALACCEL_SONAME=$<TARGET_SONAME_FILE_NAME:KF6::GlobalAccel>"
                "-DQINDAQT_TOKENS_LIBRARY=$<TARGET_FILE:qindaqt_tokens_qml>"
                "-DQINDAQT_TOKENS_SONAME=$<TARGET_SONAME_FILE_NAME:qindaqt_tokens_qml>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/run_installed_launcher.cmake"
        )
        set_tests_properties(
            qindaqt.launcher-installed-package
            PROPERTIES
                LABELS "shell;launcher;runtime;installed;poison"
                TIMEOUT 120
                ENVIRONMENT_MODIFICATION
                    "DBUS_SESSION_BUS_ADDRESS=unset:;DISPLAY=unset:;WAYLAND_DISPLAY=unset:;XDG_RUNTIME_DIR=set:${CMAKE_CURRENT_BINARY_DIR}/isolated-runtime"
        )
    endif()
endif()

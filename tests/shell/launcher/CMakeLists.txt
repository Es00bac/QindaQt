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
                 Core DBus Gui Qml Quick QuickControls2 Test)
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
    add_subdirectory(
        "../../../src/shell/launcher"
        "${CMAKE_BINARY_DIR}/src/shell/launcher"
    )
    set(launcher_source_root "${CMAKE_CURRENT_SOURCE_DIR}/../../..")
else()
    set(launcher_source_root "${CMAKE_SOURCE_DIR}")
endif()

function(qindaqt_add_shell_launcher_test target source test_name)
    qt_add_executable(${target} ${source})
    target_link_libraries(${target} PRIVATE QindaQt::ShellLauncher Qt6::Test)
    qindaqt_enable_warnings(${target})
    add_test(NAME ${test_name} COMMAND ${target})
endfunction()

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
    function(qindaqt_add_launcher_runtime_test target source test_name)
        qt_add_executable(${target} ${source})
        target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
        target_link_libraries(
            ${target}
            PRIVATE QindaQt::ShellLauncherRuntime Qt6::Test
        )
        qindaqt_enable_warnings(${target})
        add_test(NAME ${test_name} COMMAND ${target})
        set_tests_properties(${test_name} PROPERTIES LABELS "shell;launcher;runtime")
    endfunction()

    qindaqt_add_launcher_runtime_test(
        qindaqt_launcher_scanner_tests
        tst_application_scanner.cpp
        qindaqt.launcher-application-scanner
    )
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
            Qt6::Gui
            Qt6::Qml
            Qt6::Quick
            Qt6::Test
    )
    target_compile_definitions(
        qindaqt_launcher_qml_tests
        PRIVATE QINDAQT_LAUNCHER_QML_IMPORT_PATH="${CMAKE_BINARY_DIR}/qml"
    )
    qindaqt_enable_warnings(qindaqt_launcher_qml_tests)
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
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_LINUX_ACCESSIBILITY_ALWAYS_ON=1"
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
        PROPERTIES LABELS "shell;launcher;runtime;policy;hostile"
    )

    if(TARGET qindaqt_applet_runtime)
        qt_add_executable(qindaqt_launcher_installed_probe
                          tst_launcher_installed_probe.cpp)
        target_link_libraries(
            qindaqt_launcher_installed_probe
            PRIVATE QindaQt::AppletRuntime Qt6::Gui Qt6::Qml Qt6::Quick
        )
        qindaqt_enable_warnings(qindaqt_launcher_installed_probe)

        add_test(
            NAME qindaqt.launcher-installed-package
            COMMAND
                "${CMAKE_COMMAND}"
                "-DQINDAQT_CMAKE=${CMAKE_COMMAND}"
                "-DQINDAQT_BUILD_ROOT=${CMAKE_BINARY_DIR}"
                "-DQINDAQT_STAGE=${CMAKE_CURRENT_BINARY_DIR}/installed-launcher"
                "-DQINDAQT_PROBE=$<TARGET_FILE:qindaqt_launcher_installed_probe>"
                "-DQINDAQT_INSTALL_QMLDIR=${QT6_INSTALL_QML}"
                "-DQINDAQT_INSTALL_DATADIR=${CMAKE_INSTALL_DATADIR}"
                "-DQINDAQT_BUILD_QMLDIR=${CMAKE_BINARY_DIR}/qml"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/run_installed_launcher.cmake"
        )
        set_tests_properties(
            qindaqt.launcher-installed-package
            PROPERTIES LABELS "shell;launcher;runtime;installed;poison" TIMEOUT 120
        )
    endif()
endif()

# SPDX-License-Identifier: GPL-3.0-or-later

# Desktop controls tests: pure projections, each facade controller over the
# existing applet test fakes, the composition root over injected seams, the
# compiled QML module offscreen, and the include-boundary probe. Nothing here
# reaches the host session bus, a compositor, or a real process start.

find_package(Qt6 6.11 REQUIRED COMPONENTS Core DBus Gui Qml Quick QuickControls2 Svg Test)

set(desktop_controls_test_environment
    "DBUS_SESSION_BUS_ADDRESS=unset:;DISPLAY=unset:;WAYLAND_DISPLAY=unset:;XDG_RUNTIME_DIR=set:${CMAKE_CURRENT_BINARY_DIR}/isolated-runtime")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/isolated-runtime")
file(CHMOD "${CMAKE_CURRENT_BINARY_DIR}/isolated-runtime"
     PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)

set(desktop_controls_fixture_dirs
    "${PROJECT_SOURCE_DIR}/tests/shell/launcher"
    "${PROJECT_SOURCE_DIR}/tests/shell/task_list"
    "${PROJECT_SOURCE_DIR}/tests/shell/workspaces"
    "${PROJECT_SOURCE_DIR}/tests/services/power_client"
    "${PROJECT_SOURCE_DIR}/tests/services/audio_client"
    "${PROJECT_SOURCE_DIR}/tests/services/bluetooth_client"
    "${PROJECT_SOURCE_DIR}/tests/shell")

function(qindaqt_add_desktop_controls_test target source test_name)
    qt_add_executable(${target} ${source} desktop_controls_test_support.h
        "${PROJECT_SOURCE_DIR}/tests/shell/task_list/task_list_operation_test_support.h"
        "${PROJECT_SOURCE_DIR}/tests/shell/task_list/task_list_applet_test_fakes.h")
    target_include_directories(${target} PRIVATE ${desktop_controls_fixture_dirs})
    target_link_libraries(
        ${target}
        PRIVATE
            QindaQt::ShellDesktopControls
            QindaQt::ShellWorkspaces
            QindaQt::SettingsClient
            QindaQt::GlobalMenuProtocol
            Qt6::Test
    )
    target_compile_definitions(${target} PRIVATE QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    qindaqt_enable_warnings(${target})
    add_test(NAME ${test_name} COMMAND ${target})
    set_tests_properties(
        ${test_name}
        PROPERTIES
            LABELS "shell;desktop-controls"
            ENVIRONMENT_MODIFICATION "${desktop_controls_test_environment}"
    )
endfunction()

qindaqt_add_desktop_controls_test(qindaqt_desktop_controls_values_tests
    tst_desktop_controls_values.cpp qindaqt.desktop-controls-values)
qindaqt_add_desktop_controls_test(qindaqt_desktop_controls_status_tests
    tst_system_status_controller.cpp qindaqt.desktop-controls-system-status)
qindaqt_add_desktop_controls_test(qindaqt_desktop_controls_menu_places_tests
    tst_system_menu_and_places.cpp qindaqt.desktop-controls-system-menu-places)
qindaqt_add_desktop_controls_test(qindaqt_desktop_controls_launcher_windows_tests
    tst_quick_launch_and_active_application.cpp
    qindaqt.desktop-controls-quick-launch-active-application)
# The naming row resolves real desktop entries through the shell icon module.
target_link_libraries(qindaqt_desktop_controls_launcher_windows_tests PRIVATE QindaQt::ShellIcons)
qindaqt_add_desktop_controls_test(qindaqt_desktop_controls_search_tests
    tst_command_search_controller.cpp qindaqt.desktop-controls-command-search)

# Composition root over injected seams. Compiles the shell-private runtime
# composition directly (launcher composition test precedent).
if(TARGET qindaqt-shell)
    qt_add_executable(
        qindaqt_desktop_controls_composition_tests
        tst_desktop_controls_composition.cpp
        desktop_controls_test_support.h
        "${PROJECT_SOURCE_DIR}/src/shell/runtime/desktopcontrolscomposition.cpp"
        "${PROJECT_SOURCE_DIR}/src/shell/runtime/desktopcontrolscomposition.h"
    )
    target_include_directories(
        qindaqt_desktop_controls_composition_tests
        PRIVATE "${PROJECT_SOURCE_DIR}/src/shell/runtime" ${desktop_controls_fixture_dirs}
    )
    target_link_libraries(
        qindaqt_desktop_controls_composition_tests
        PRIVATE
            QindaQt::ShellDesktopControls
            QindaQt::ShellWorkspacesDBus
            QindaQt::SettingsClient
            QindaQt::GlobalMenuProtocol
            Qt6::DBus
            Qt6::Test
    )
    target_compile_definitions(
        qindaqt_desktop_controls_composition_tests
        PRIVATE QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
    )
    qindaqt_enable_warnings(qindaqt_desktop_controls_composition_tests)
    add_test(NAME qindaqt.desktop-controls-composition
             COMMAND qindaqt_desktop_controls_composition_tests)
    set_tests_properties(
        qindaqt.desktop-controls-composition
        PROPERTIES
            LABELS "shell;desktop-controls;composition"
            ENVIRONMENT_MODIFICATION "${desktop_controls_test_environment}"
    )
endif()

# Compiled module offscreen: keyboard, accessibility, and separate popup
# windows, driven through real controllers over the fakes above.
function(qindaqt_add_desktop_controls_qml_test target source test_name)
    qt_add_executable(${target} ${source}
        desktop_controls_test_support.h desktop_controls_qml_test_support.h
        "${PROJECT_SOURCE_DIR}/tests/shell/task_list/task_list_operation_test_support.h"
        "${PROJECT_SOURCE_DIR}/tests/shell/task_list/task_list_applet_test_fakes.h")
    target_include_directories(${target} PRIVATE ${desktop_controls_fixture_dirs})
    target_link_libraries(
        ${target}
        PRIVATE
            QindaQt::ShellDesktopControlsRuntime
            qindaqt_shell_desktop_controls_runtimeplugin
            QindaQt::ShellWorkspaces
            QindaQt::SettingsClient
            QindaQt::GlobalMenuProtocol
            QindaQt::TokensQml
            QindaQt::ShellIcons
            qindaqt_shell_iconsplugin
            Qt6::Gui
            Qt6::Qml
            Qt6::Quick
            Qt6::Test
    )
    target_compile_definitions(
        ${target}
        PRIVATE
            QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
            QINDAQT_DESKTOP_CONTROLS_QML_IMPORT_PATH="${CMAKE_BINARY_DIR}/qml"
    )
    add_dependencies(${target} qindaqt_tokens_qmlplugin qindaqt_controls_qmlplugin)
    qindaqt_enable_warnings(${target})
    qt_import_qml_plugins(${target})
    add_test(NAME ${test_name} COMMAND ${target})
    set_tests_properties(
        ${test_name}
        PROPERTIES
            LABELS "shell;desktop-controls;offscreen;accessibility"
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_FATAL_WARNINGS=1"
            ENVIRONMENT_MODIFICATION "${desktop_controls_test_environment}"
    )
endfunction()

qindaqt_add_desktop_controls_qml_test(qindaqt_desktop_controls_qml_workspace_tests
    tst_desktop_controls_qml_workspaces.cpp qindaqt.desktop-controls-offscreen-workspaces)
qindaqt_add_desktop_controls_qml_test(qindaqt_desktop_controls_qml_menu_tests
    tst_desktop_controls_qml_menus.cpp qindaqt.desktop-controls-offscreen-menus)

add_test(
    NAME qindaqt.desktop-controls-boundary
    COMMAND "${CMAKE_COMMAND}"
        "-DSOURCE_ROOT=${PROJECT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/check_desktop_controls_boundary.cmake"
)
set_tests_properties(qindaqt.desktop-controls-boundary
    PROPERTIES LABELS "shell;desktop-controls;policy")

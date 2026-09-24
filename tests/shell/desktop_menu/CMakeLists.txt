# SPDX-License-Identifier: GPL-3.0-or-later

# ADR-0260 desktop menu rows: the pure builder against File Manager's public
# catalog and the canonical bounds, selection/activation against the real
# global-menu facade, and the desktop-icons command channel. The runtime
# composition row (routing to the real controllers) lives with the shell
# runtime tests.

find_package(Qt6 6.11 REQUIRED COMPONENTS Core Test)

function(qindaqt_add_desktop_menu_test target source test_name)
    qt_add_executable(${target} ${source} desktop_menu_test_support.h)
    target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(
        ${target}
        PRIVATE
            QindaQt::ShellDesktopMenu
            QindaQt::GlobalMenuApplet
            QindaQt::GlobalMenuProtocol
            qindaqt_file_manager_menu_catalog
            Qt6::Test
    )
    target_compile_definitions(${target} PRIVATE QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    qindaqt_enable_warnings(${target})
    add_test(NAME ${test_name} COMMAND ${target})
    set_tests_properties(${test_name} PROPERTIES LABELS "shell;global-menu;desktop-menu")
endfunction()

qindaqt_add_desktop_menu_test(qindaqt_desktop_menu_model_tests
    tst_desktop_menu_model.cpp qindaqt.desktop-menu-model)
qindaqt_add_desktop_menu_test(qindaqt_desktop_menu_controller_tests
    tst_desktop_menu_controller.cpp qindaqt.desktop-menu-controller)
qindaqt_add_desktop_menu_test(qindaqt_desktop_surface_commands_tests
    tst_desktop_surface_commands.cpp qindaqt.desktop-menu-surface-commands)

# Shell-private runtime composition rows. They compile the runtime sources
# directly (desktop-controls composition precedent) and exist only where the
# production shell builds.
if(TARGET qindaqt-shell)
    find_package(Qt6 6.11 REQUIRED COMPONENTS DBus)
    find_program(QINDAQT_DBUS_RUN_SESSION dbus-run-session REQUIRED)

    # Routing: every command reaches its owning controller over the shared
    # desktop-controls doubles (spawner, folder opener, workspace transport,
    # session actions).
    qt_add_executable(
        qindaqt_desktop_menu_targets_tests
        tst_desktop_menu_targets.cpp
        "${PROJECT_SOURCE_DIR}/tests/shell/desktop_controls/desktop_controls_test_support.h"
        "${PROJECT_SOURCE_DIR}/src/shell/runtime/desktopmenutargets.cpp"
        "${PROJECT_SOURCE_DIR}/src/shell/runtime/desktopmenutargets.h"
    )
    target_include_directories(
        qindaqt_desktop_menu_targets_tests
        PRIVATE
            "${PROJECT_SOURCE_DIR}/src/shell/runtime"
            "${PROJECT_SOURCE_DIR}/tests/shell/desktop_controls"
            "${PROJECT_SOURCE_DIR}/tests/shell/launcher"
            "${PROJECT_SOURCE_DIR}/tests/shell/task_list"
            "${PROJECT_SOURCE_DIR}/tests/shell/workspaces"
            "${PROJECT_SOURCE_DIR}/tests/shell"
    )
    target_link_libraries(
        qindaqt_desktop_menu_targets_tests
        PRIVATE
            QindaQt::ShellDesktopMenu
            QindaQt::ShellDesktopControls
            QindaQt::ShellClipboardAppletRuntime
            QindaQt::ShellWorkspaces
            QindaQt::SettingsClient
            QindaQt::GlobalMenuProtocol
            Qt6::Test
    )
    target_compile_definitions(
        qindaqt_desktop_menu_targets_tests
        PRIVATE QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
    )
    qindaqt_enable_warnings(qindaqt_desktop_menu_targets_tests)
    add_test(NAME qindaqt.desktop-menu-targets COMMAND qindaqt_desktop_menu_targets_tests)
    set_tests_properties(
        qindaqt.desktop-menu-targets
        PROPERTIES
            LABELS "shell;global-menu;desktop-menu;desktop-controls;composition"
            ENVIRONMENT_MODIFICATION
                "DBUS_SESSION_BUS_ADDRESS=unset:;DISPLAY=unset:;WAYLAND_DISPLAY=unset:"
    )

    # Selection: the production global-menu composition and the desktop menu
    # composition over one facade and one identity client, on a private bus,
    # plus every stock profile resolved through the panel dispatcher's path.
    qt_add_executable(
        qindaqt_desktop_menu_selection_tests
        tst_desktop_menu_selection_private_bus.cpp
        "${PROJECT_SOURCE_DIR}/tests/shell/global_menu/dbusmenu/fake_dbusmenu_exporter.h"
        "${PROJECT_SOURCE_DIR}/src/shell/runtime/desktopmenucomposition.cpp"
        "${PROJECT_SOURCE_DIR}/src/shell/runtime/desktopmenucomposition.h"
        "${PROJECT_SOURCE_DIR}/src/shell/runtime/desktopmenutargets.cpp"
        "${PROJECT_SOURCE_DIR}/src/shell/runtime/desktopmenutargets.h"
        "${PROJECT_SOURCE_DIR}/src/shell/runtime/globalmenuappletcomposition.cpp"
        "${PROJECT_SOURCE_DIR}/src/shell/runtime/globalmenuappletcomposition.h"
    )
    target_include_directories(
        qindaqt_desktop_menu_selection_tests
        PRIVATE
            "${PROJECT_SOURCE_DIR}/src/shell/runtime"
            "${PROJECT_SOURCE_DIR}/tests/shell/global_menu/dbusmenu"
    )
    target_link_libraries(
        qindaqt_desktop_menu_selection_tests
        PRIVATE
            QindaQt::ShellDesktopMenu
            QindaQt::ShellDesktopControls
            QindaQt::ShellClipboardAppletRuntime
            QindaQt::AppletHost
            QindaQt::AppletRuntime
            QindaQt::GlobalMenuApplet
            QindaQt::GlobalMenuDbusMenu
            QindaQt::GlobalMenuRegistrar
            QindaQt::GlobalMenuTransportComposition
            QindaQt::ShellWindowActionsClient
            qindaqt_profiles
            Qt6::Core
            Qt6::DBus
            Qt6::Test
    )
    target_compile_definitions(
        qindaqt_desktop_menu_selection_tests
        PRIVATE QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
    )
    qindaqt_enable_warnings(qindaqt_desktop_menu_selection_tests)
    add_test(
        NAME qindaqt.desktop-menu-selection-private-bus
        COMMAND "${QINDAQT_DBUS_RUN_SESSION}" --
                $<TARGET_FILE:qindaqt_desktop_menu_selection_tests>
    )
    set_tests_properties(
        qindaqt.desktop-menu-selection-private-bus
        PROPERTIES
            TIMEOUT 60
            LABELS "shell;global-menu;desktop-menu;runtime;private-bus;profiles"
    )
endif()

# Keyboard navigation of the bar: the compiled QindaQt.Shell.GlobalMenu
# renderer over the real facade and the real desktop tree, offscreen, with
# QML warnings fatal.
find_package(Qt6 6.11 REQUIRED COMPONENTS Qml Quick QuickTest)
qt_add_executable(
    qindaqt_desktop_menu_keyboard_tests
    desktop_menu_qml_main.cpp
    desktop_menu_test_support.h
)
target_include_directories(qindaqt_desktop_menu_keyboard_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(
    qindaqt_desktop_menu_keyboard_tests
    PRIVATE
        QindaQt::ShellDesktopMenu
        QindaQt::GlobalMenuApplet
        QindaQt::GlobalMenuProtocol
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickTest
)
qindaqt_enable_warnings(qindaqt_desktop_menu_keyboard_tests)
add_dependencies(qindaqt_desktop_menu_keyboard_tests qindaqt_global_menu_qmlplugin)
add_test(
    NAME qindaqt.desktop-menu-keyboard-qml-offscreen
    COMMAND
        qindaqt_desktop_menu_keyboard_tests
        -import "${CMAKE_BINARY_DIR}/qml"
        -input "${CMAKE_CURRENT_SOURCE_DIR}/qml"
)
set_tests_properties(
    qindaqt.desktop-menu-keyboard-qml-offscreen
    PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_FATAL_WARNINGS=1"
        LABELS "shell;global-menu;desktop-menu;offscreen;keyboard"
        FIXTURES_REQUIRED qindaqt_global_menu_build_tree_module
)

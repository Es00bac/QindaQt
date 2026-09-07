# SPDX-License-Identifier: GPL-3.0-or-later

# The production adapter is a static library whose private qtermwidget link is
# represented as a link-only final-consumer requirement. Re-import the same
# pinned target in this sibling directory so the one real-adapter test can
# resolve that requirement without leaking headers into support tests.
find_package(qtermwidget6 2.4...<2.5 REQUIRED)

function(qindaqt_add_terminal_test target source test_name)
    qt_add_executable(${target} ${source})
    target_link_libraries(${target} PRIVATE qindaqt_terminal_support Qt6::Test)
    target_compile_features(${target} PRIVATE cxx_std_20)
    set_target_properties(${target} PROPERTIES CXX_EXTENSIONS OFF)
    qindaqt_enable_warnings(${target})
    add_test(NAME ${test_name} COMMAND ${target})
    # AGENT-GUARD (P2: headless rows): the support library PUBLIC-links
    # Qt6::Widgets, so QTEST_MAIN instantiates QApplication in every row
    # linked this way. Without an explicit offscreen platform each row aborts
    # in display-less CI before its first assertion. Any new row registered
    # through this helper inherits the headless-safe environment.
    set_tests_properties(${test_name}
        PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endfunction()

qindaqt_add_terminal_test(
    qindaqt_terminal_launch_policy_tests
    tst_launch_policy.cpp
    qindaqt.terminal-launch-policy
)
qindaqt_add_terminal_test(
    qindaqt_terminal_pty_bridge_tests
    tst_pty_bridge.cpp
    qindaqt.terminal-pty-bridge
)
qindaqt_add_terminal_test(
    qindaqt_terminal_session_tests
    tst_terminal_session.cpp
    qindaqt.terminal-session
)
qindaqt_add_terminal_test(
    qindaqt_terminal_process_group_tests
    tst_terminal_process_group.cpp
    qindaqt.terminal-process-group
)
qindaqt_add_terminal_test(
    qindaqt_terminal_appearance_tests
    tst_terminal_appearance.cpp
    qindaqt.terminal-appearance
)
target_compile_definitions(
    qindaqt_terminal_appearance_tests
    PRIVATE QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)

qindaqt_add_terminal_test(
    qindaqt_terminal_profiles_tests
    tst_terminal_profiles.cpp
    qindaqt.terminal-profiles
)
qindaqt_add_terminal_test(
    qindaqt_terminal_session_collection_tests
    tst_terminal_session_collection.cpp
    qindaqt.terminal-session-collection
)
qindaqt_add_terminal_test(
    qindaqt_terminal_profile_settings_tests
    tst_terminal_profile_settings.cpp
    qindaqt.terminal-profile-settings
)
target_link_libraries(
    qindaqt_terminal_profile_settings_tests PRIVATE qindaqt_settings_client
)

qindaqt_add_terminal_test(
    qindaqt_terminal_window_tests
    tst_terminal_window.cpp
    qindaqt.terminal-window-offscreen
)
target_compile_definitions(
    qindaqt_terminal_window_tests
    PRIVATE QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)
qindaqt_add_terminal_test(
    qindaqt_terminal_app_shell_tests
    tst_terminal_app_shell.cpp
    qindaqt.terminal-app-shell
)
qindaqt_add_terminal_test(
    qindaqt_terminal_search_tests
    tst_terminal_search.cpp
    qindaqt.terminal-search-policy
)
qindaqt_add_terminal_test(
    qindaqt_terminal_links_tests
    tst_terminal_links.cpp
    qindaqt.terminal-links
)
qindaqt_add_terminal_test(
    qindaqt_terminal_search_links_ui_tests
    tst_terminal_search_links_ui.cpp
    qindaqt.terminal-search-links-offscreen
)
target_compile_definitions(
    qindaqt_terminal_search_links_ui_tests
    PRIVATE QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)
set_tests_properties(
    qindaqt.terminal-search-links-offscreen
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_FATAL_WARNINGS=1"
)
qindaqt_add_terminal_test(
    qindaqt_terminal_adapter_boundary_tests
    tst_terminal_adapter_boundary.cpp
    qindaqt.terminal-adapter-boundary
)
qindaqt_add_terminal_test(
    qindaqt_terminal_container_tests
    tst_terminal_container.cpp
    qindaqt.terminal-container-offscreen
)
target_compile_definitions(
    qindaqt_terminal_container_tests
    PRIVATE QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)
set_tests_properties(
    qindaqt.terminal-container-offscreen
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_FATAL_WARNINGS=1"
)

# This is intentionally the only focused test that links the production
# qtermwidget adapter. It locks down custom-scheme lookup/application and the
# real widget's blank-grid selection representation; fakes cannot reproduce
# either upstream boundary.
qt_add_executable(
    qindaqt_terminal_widget_adapter_tests
    tst_terminal_widget_adapter.cpp
)
target_link_libraries(
    qindaqt_terminal_widget_adapter_tests
    PRIVATE qindaqt_terminal_adapter qtermwidget6 Qt6::Test
)
target_compile_features(qindaqt_terminal_widget_adapter_tests PRIVATE cxx_std_20)
set_target_properties(
    qindaqt_terminal_widget_adapter_tests PROPERTIES CXX_EXTENSIONS OFF
)
qindaqt_enable_warnings(qindaqt_terminal_widget_adapter_tests)
target_compile_definitions(
    qindaqt_terminal_widget_adapter_tests
    PRIVATE QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)
add_test(
    NAME qindaqt.terminal-widget-adapter-offscreen
    COMMAND qindaqt_terminal_widget_adapter_tests
)
set_tests_properties(
    qindaqt.terminal-widget-adapter-offscreen
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

add_test(
    NAME qindaqt.terminal-desktop-metadata
    COMMAND "${CMAKE_COMMAND}"
            "-DDESKTOP_FILE=${PROJECT_SOURCE_DIR}/src/apps/terminal/org.qindaqt.Terminal.desktop"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/check_desktop_metadata.cmake"
)
add_test(
    NAME qindaqt.terminal-cli-rejects-positional-arguments
    COMMAND "${CMAKE_COMMAND}"
            "-DTERMINAL_EXECUTABLE=$<TARGET_FILE:qindaqt-terminal>"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/check_cli_rejection.cmake"
)
set_tests_properties(
    qindaqt.terminal-cli-rejects-positional-arguments
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

# AGENT-CONTRACT: This row resolves themes and metadata only from a clean
# staged prefix; build-tree RPATHs or source data cannot satisfy the gate.
add_test(
    NAME qindaqt.terminal-installed-metadata
    COMMAND "${CMAKE_COMMAND}"
            "-DBUILD_DIRECTORY=${CMAKE_BINARY_DIR}"
            "-DINSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/installed-terminal-stage"
            "-DINSTALL_BINDIR=${CMAKE_INSTALL_BINDIR}"
            "-DINSTALL_DATADIR=${CMAKE_INSTALL_DATADIR}"
            "-DQTERMWIDGET_LIBRARY=$<TARGET_FILE:qtermwidget6>"
            "-DCONFIGURATION=$<CONFIG>"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_installed_terminal.cmake"
)

# First-party global-menu export rows. They mirror the accepted File Manager
# row shape (tests/shell/global_menu/runtime_composition): the real Terminal
# process runs offscreen against the production GlobalMenuAppletComposition on
# one private bus, with the test seam providing the exact registrar window id.
find_program(QINDAQT_TERMINAL_MENU_DBUS_RUN_SESSION dbus-run-session REQUIRED)
qt_add_executable(
    qindaqt_terminal_menu_export_tests
    tst_terminal_menu_export.cpp
    "${PROJECT_SOURCE_DIR}/src/shell/runtime/globalmenuappletcomposition.cpp"
    "${PROJECT_SOURCE_DIR}/src/shell/runtime/globalmenuappletcomposition.h"
)
target_include_directories(
    qindaqt_terminal_menu_export_tests
    PRIVATE "${PROJECT_SOURCE_DIR}/src/shell/runtime"
)
target_compile_definitions(
    qindaqt_terminal_menu_export_tests
    PRIVATE
        QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
        QINDAQT_TERMINAL="$<TARGET_FILE:qindaqt-terminal>"
        QINDAQT_TEST_ISOLATION_ROOT="${CMAKE_CURRENT_BINARY_DIR}/menu-export-isolated"
)
add_dependencies(qindaqt_terminal_menu_export_tests qindaqt-terminal)
target_link_libraries(
    qindaqt_terminal_menu_export_tests
    PRIVATE
        QindaQt::AppletHost
        QindaQt::AppletRuntime
        QindaQt::GlobalMenuApplet
        QindaQt::GlobalMenuDbusMenu
        QindaQt::GlobalMenuRegistrar
        QindaQt::GlobalMenuTransportComposition
        QindaQt::ShellWindowActionsClient
        Qt6::Core
        Qt6::DBus
        Qt6::Test
)
qindaqt_enable_warnings(qindaqt_terminal_menu_export_tests)
add_test(
    NAME qindaqt.terminal-global-menu-shell-private-bus
    COMMAND "${QINDAQT_TERMINAL_MENU_DBUS_RUN_SESSION}" --
            $<TARGET_FILE:qindaqt_terminal_menu_export_tests>
            shellFencesRealTerminalIdentity
)
set_tests_properties(
    qindaqt.terminal-global-menu-shell-private-bus
    PROPERTIES
        TIMEOUT 60
        LABELS "terminal;shell;global-menu;runtime;private-bus;offscreen;hostile"
        ENVIRONMENT "DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent"
        ENVIRONMENT_MODIFICATION
            "DISPLAY=unset:;WAYLAND_DISPLAY=unset:;DBUS_SESSION_BUS_ADDRESS=unset:"
)
add_test(
    NAME qindaqt.terminal-global-menu-registrar-absent-private-bus
    COMMAND "${QINDAQT_TERMINAL_MENU_DBUS_RUN_SESSION}" --
            $<TARGET_FILE:qindaqt_terminal_menu_export_tests>
            staysFailClosedUntilRegistrarArrives
)
set_tests_properties(
    qindaqt.terminal-global-menu-registrar-absent-private-bus
    PROPERTIES
        TIMEOUT 60
        LABELS "terminal;shell;global-menu;runtime;private-bus;offscreen"
        ENVIRONMENT "DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent"
        ENVIRONMENT_MODIFICATION
            "DISPLAY=unset:;WAYLAND_DISPLAY=unset:;DBUS_SESSION_BUS_ADDRESS=unset:"
)
add_test(
    NAME qindaqt.terminal-global-menu-hostile-registrar-private-bus
    COMMAND "${QINDAQT_TERMINAL_MENU_DBUS_RUN_SESSION}" --
            $<TARGET_FILE:qindaqt_terminal_menu_export_tests>
            failsClosedUnderHostileRegistrar
)
set_tests_properties(
    qindaqt.terminal-global-menu-hostile-registrar-private-bus
    PROPERTIES
        TIMEOUT 60
        LABELS "terminal;shell;global-menu;runtime;private-bus;offscreen;hostile"
        ENVIRONMENT "DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent"
        ENVIRONMENT_MODIFICATION
            "DISPLAY=unset:;WAYLAND_DISPLAY=unset:;DBUS_SESSION_BUS_ADDRESS=unset:"
)
add_test(
    NAME qindaqt.terminal-global-menu-identity-variants-source-policy
    COMMAND
        "${CMAKE_COMMAND}"
        -DSOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tst_terminal_menu_export.cpp
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_menu_export_identity_variants.cmake
)
set_tests_properties(
    qindaqt.terminal-global-menu-identity-variants-source-policy
    PROPERTIES LABELS "terminal;shell;global-menu;policy"
)

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
    qindaqt_terminal_appearance_tests
    tst_terminal_appearance.cpp
    qindaqt.terminal-appearance
)
target_compile_definitions(
    qindaqt_terminal_appearance_tests
    PRIVATE QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
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
    PRIVATE qindaqt_terminal_adapter Qt6::Test
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
            "-DCONFIGURATION=$<CONFIG>"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_installed_terminal.cmake"
)

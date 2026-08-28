# SPDX-License-Identifier: GPL-3.0-or-later

function(qindaqt_add_terminal_test target source test_name)
    qt_add_executable(${target} ${source})
    target_link_libraries(${target} PRIVATE qindaqt_terminal_support Qt6::Test)
    target_compile_features(${target} PRIVATE cxx_std_20)
    set_target_properties(${target} PROPERTIES CXX_EXTENSIONS OFF)
    qindaqt_enable_warnings(${target})
    add_test(NAME ${test_name} COMMAND ${target})
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
set_tests_properties(
    qindaqt.terminal-window-offscreen
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

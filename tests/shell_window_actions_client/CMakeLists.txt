# SPDX-License-Identifier: GPL-3.0-or-later

find_program(DBUS_RUN_SESSION_EXECUTABLE dbus-run-session REQUIRED)

qt_add_executable(
    qindaqt_shell_window_actions_client_tests
    tst_shellwindowactionsclient.cpp
)
target_link_libraries(
    qindaqt_shell_window_actions_client_tests
    PRIVATE QindaQt::ShellWindowActionsClient Qt6::Test
)
add_test(
    NAME qindaqt.shell-window-actions-client
    COMMAND qindaqt_shell_window_actions_client_tests
)

qt_add_executable(
    qindaqt_shell_window_actions_private_bus_tests
    tst_shellwindowactionsprivatebus.cpp
)
target_link_libraries(
    qindaqt_shell_window_actions_private_bus_tests
    PRIVATE QindaQt::ShellWindowActionsClient Qt6::DBus Qt6::Test
)
add_test(
    NAME qindaqt.shell-window-actions-private-bus
    COMMAND "${DBUS_RUN_SESSION_EXECUTABLE}" --
            "$<TARGET_FILE:qindaqt_shell_window_actions_private_bus_tests>"
)
set_tests_properties(
    qindaqt.shell-window-actions-private-bus
    PROPERTIES ENVIRONMENT "DISPLAY=;WAYLAND_DISPLAY="
)

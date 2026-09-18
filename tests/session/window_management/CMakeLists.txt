# SPDX-License-Identifier: GPL-3.0-or-later
# The windowManagement.* live bridge (ADR-0209).
find_program(DBUS_DAEMON_EXECUTABLE dbus-daemon REQUIRED)

qt_add_executable(
    qindaqt_window_management_preferences_tests
    tst_window_management_preferences.cpp
)
target_link_libraries(
    qindaqt_window_management_preferences_tests
    PRIVATE QindaQt::WindowManagementBridge Qt6::Test
)
qindaqt_enable_warnings(qindaqt_window_management_preferences_tests)
add_test(
    NAME qindaqt.window-management-preferences
    COMMAND qindaqt_window_management_preferences_tests
)

qt_add_executable(
    qindaqt_kwin_window_management_writer_tests
    tst_kwin_window_management_writer.cpp
)
target_link_libraries(
    qindaqt_kwin_window_management_writer_tests
    PRIVATE QindaQt::WindowManagementBridge Qt6::Test
)
qindaqt_enable_warnings(qindaqt_kwin_window_management_writer_tests)
add_test(
    NAME qindaqt.kwin-window-management-writer
    COMMAND qindaqt_kwin_window_management_writer_tests
)

qt_add_executable(
    qindaqt_window_management_bridge_tests
    tst_window_management_bridge.cpp
)
target_link_libraries(
    qindaqt_window_management_bridge_tests
    PRIVATE
        QindaQt::WindowManagementBridge
        QindaQt::SettingsService
        Qt6::DBus
        Qt6::Test
)
target_compile_definitions(
    qindaqt_window_management_bridge_tests
    PRIVATE
        QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
        QINDAQT_DBUS_DAEMON_EXECUTABLE="${DBUS_DAEMON_EXECUTABLE}"
)
qindaqt_enable_warnings(qindaqt_window_management_bridge_tests)
add_test(
    NAME qindaqt.window-management-bridge
    COMMAND qindaqt_window_management_bridge_tests
)

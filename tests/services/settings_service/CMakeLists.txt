# SPDX-License-Identifier: GPL-3.0-or-later
find_program(DBUS_DAEMON_EXECUTABLE dbus-daemon REQUIRED)

qt_add_executable(qindaqt_settings_repository_tests tst_settings_repository.cpp)
target_link_libraries(qindaqt_settings_repository_tests PRIVATE QindaQt::SettingsService Qt6::Test)
target_compile_definitions(qindaqt_settings_repository_tests PRIVATE QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
qindaqt_enable_warnings(qindaqt_settings_repository_tests)
add_test(NAME qindaqt.settings-repository COMMAND qindaqt_settings_repository_tests)

qt_add_executable(qindaqt_settings_service_lifecycle_tests tst_settings_service_lifecycle.cpp)
target_link_libraries(qindaqt_settings_service_lifecycle_tests PRIVATE QindaQt::SettingsService Qt6::Test Qt6::DBus)
target_compile_definitions(
    qindaqt_settings_service_lifecycle_tests
    PRIVATE QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}" QINDAQT_DBUS_DAEMON_EXECUTABLE="${DBUS_DAEMON_EXECUTABLE}"
)
qindaqt_enable_warnings(qindaqt_settings_service_lifecycle_tests)
add_test(NAME qindaqt.settings-service-lifecycle COMMAND qindaqt_settings_service_lifecycle_tests)

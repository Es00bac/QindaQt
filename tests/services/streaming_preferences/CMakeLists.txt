# SPDX-License-Identifier: LGPL-3.0-or-later
qt_add_executable(qindaqt_streaming_preferences_tests tst_streaming_preferences.cpp)
target_link_libraries(qindaqt_streaming_preferences_tests
    PRIVATE QindaQt::StreamingPreferences Qt6::Test)
qindaqt_enable_warnings(qindaqt_streaming_preferences_tests)
add_test(NAME qindaqt.services-streaming-preferences
    COMMAND qindaqt_streaming_preferences_tests)
set_tests_properties(qindaqt.services-streaming-preferences PROPERTIES
    LABELS "services;streaming;settings")

find_program(DBUS_DAEMON_EXECUTABLE dbus-daemon REQUIRED)
qt_add_executable(qindaqt_obs_login_helper_tests tst_obs_login_helper.cpp)
target_link_libraries(qindaqt_obs_login_helper_tests PRIVATE
    QindaQt::SettingsClient QindaQt::SettingsService Qt6::Test Qt6::DBus)
target_compile_definitions(qindaqt_obs_login_helper_tests PRIVATE
    QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
    QINDAQT_DBUS_DAEMON_EXECUTABLE="${DBUS_DAEMON_EXECUTABLE}"
    QINDAQT_OBS_LOGIN_EXECUTABLE="$<TARGET_FILE:qindaqt-obs-login>")
add_dependencies(qindaqt_obs_login_helper_tests qindaqt-obs-login)
qindaqt_enable_warnings(qindaqt_obs_login_helper_tests)
add_test(NAME qindaqt.services-obs-login-helper
    COMMAND qindaqt_obs_login_helper_tests)
set_tests_properties(qindaqt.services-obs-login-helper PROPERTIES
    LABELS "services;streaming;session;dbus")

# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_executable(qindaqt_desktop_shortcut_set_tests tst_desktop_shortcut_set.cpp)
target_link_libraries(qindaqt_desktop_shortcut_set_tests
    PRIVATE qindaqt_desktop_controls Qt6::Test)
qindaqt_enable_warnings(qindaqt_desktop_shortcut_set_tests)
add_test(NAME qindaqt.session-desktop-controls-shortcut-set
    COMMAND qindaqt_desktop_shortcut_set_tests)
set_tests_properties(qindaqt.session-desktop-controls-shortcut-set PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    LABELS "session;session-desktop-controls;model")

qt_add_executable(qindaqt_volume_key_controller_tests
    tst_volume_key_controller.cpp fake_audio_transport.h)
target_link_libraries(qindaqt_volume_key_controller_tests
    PRIVATE qindaqt_desktop_controls Qt6::Test)
qindaqt_enable_warnings(qindaqt_volume_key_controller_tests)
add_test(NAME qindaqt.session-desktop-controls-volume-keys
    COMMAND qindaqt_volume_key_controller_tests)
set_tests_properties(qindaqt.session-desktop-controls-volume-keys PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    LABELS "session;session-desktop-controls;model")

qt_add_executable(qindaqt_brightness_key_controller_tests
    tst_brightness_key_controller.cpp)
target_link_libraries(qindaqt_brightness_key_controller_tests
    PRIVATE qindaqt_desktop_controls Qt6::Test)
target_compile_definitions(qindaqt_brightness_key_controller_tests PRIVATE
    QINDAQT_TEST_SCRATCH_DIR="${CMAKE_CURRENT_BINARY_DIR}/scratch")
qindaqt_enable_warnings(qindaqt_brightness_key_controller_tests)
add_test(NAME qindaqt.session-desktop-controls-brightness-keys
    COMMAND qindaqt_brightness_key_controller_tests)
set_tests_properties(qindaqt.session-desktop-controls-brightness-keys PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    LABELS "session;session-desktop-controls;model")

qt_add_executable(qindaqt_idle_display_preferences_tests
    tst_idle_display_preferences.cpp)
target_link_libraries(qindaqt_idle_display_preferences_tests
    PRIVATE qindaqt_desktop_controls Qt6::Test)
qindaqt_enable_warnings(qindaqt_idle_display_preferences_tests)
add_test(NAME qindaqt.session-desktop-controls-idle-preferences
    COMMAND qindaqt_idle_display_preferences_tests)
set_tests_properties(qindaqt.session-desktop-controls-idle-preferences PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    LABELS "session;session-desktop-controls;model")

qt_add_executable(qindaqt_idle_display_policy_tests tst_idle_display_policy.cpp)
target_link_libraries(qindaqt_idle_display_policy_tests
    PRIVATE qindaqt_desktop_controls Qt6::Test)
qindaqt_enable_warnings(qindaqt_idle_display_policy_tests)
add_test(NAME qindaqt.session-desktop-controls-idle-policy
    COMMAND qindaqt_idle_display_policy_tests)
set_tests_properties(qindaqt.session-desktop-controls-idle-policy PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    LABELS "session;session-desktop-controls;model")

qt_add_executable(qindaqt_screenshot_launcher_tests tst_screenshot_launcher.cpp)
target_link_libraries(qindaqt_screenshot_launcher_tests
    PRIVATE qindaqt_desktop_controls Qt6::Test)
qindaqt_enable_warnings(qindaqt_screenshot_launcher_tests)
add_test(NAME qindaqt.session-desktop-controls-screenshot-launcher
    COMMAND qindaqt_screenshot_launcher_tests)
set_tests_properties(qindaqt.session-desktop-controls-screenshot-launcher PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    LABELS "session;session-desktop-controls;model")

qt_add_executable(qindaqt_settings1_idle_preferences_tests
    tst_settings1_idle_preferences.cpp)
target_link_libraries(qindaqt_settings1_idle_preferences_tests
    PRIVATE qindaqt_desktop_controls Qt6::Test)
qindaqt_enable_warnings(qindaqt_settings1_idle_preferences_tests)
add_test(NAME qindaqt.session-desktop-controls-settings1-idle-preferences
    COMMAND qindaqt_settings1_idle_preferences_tests)
set_tests_properties(qindaqt.session-desktop-controls-settings1-idle-preferences PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    LABELS "session;session-desktop-controls;model")

qt_add_executable(qindaqt_powerdevil_idle_preferences_binding_tests
    tst_powerdevil_idle_preferences_binding.cpp)
target_link_libraries(qindaqt_powerdevil_idle_preferences_binding_tests
    PRIVATE qindaqt_desktop_controls QindaQt::PowerDevilIdle Qt6::DBus Qt6::Test)
qindaqt_enable_warnings(qindaqt_powerdevil_idle_preferences_binding_tests)
add_test(NAME qindaqt.session-desktop-controls-powerdevil-binding
    COMMAND dbus-run-session -- $<TARGET_FILE:qindaqt_powerdevil_idle_preferences_binding_tests>)
set_tests_properties(qindaqt.session-desktop-controls-powerdevil-binding PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent"
    TIMEOUT 45
    LABELS "session;session-desktop-controls;dbus;power")

qt_add_executable(qindaqt_powerdevil_brightness_feedback_observer_tests
    tst_powerdevil_brightness_feedback_observer.cpp)
target_link_libraries(qindaqt_powerdevil_brightness_feedback_observer_tests
    PRIVATE qindaqt_desktop_controls Qt6::DBus Qt6::Test)
qindaqt_enable_warnings(qindaqt_powerdevil_brightness_feedback_observer_tests)
add_test(NAME qindaqt.session-desktop-controls-powerdevil-brightness-feedback
    COMMAND dbus-run-session -- $<TARGET_FILE:qindaqt_powerdevil_brightness_feedback_observer_tests>)
set_tests_properties(qindaqt.session-desktop-controls-powerdevil-brightness-feedback PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent"
    TIMEOUT 45
    LABELS "session;session-desktop-controls;dbus;power")

qt_add_executable(qindaqt_freedesktop_feedback_notifier_tests
    tst_freedesktop_feedback_notifier.cpp)
target_link_libraries(qindaqt_freedesktop_feedback_notifier_tests
    PRIVATE qindaqt_desktop_controls Qt6::DBus Qt6::Test)
qindaqt_enable_warnings(qindaqt_freedesktop_feedback_notifier_tests)
add_test(NAME qindaqt.session-desktop-controls-feedback-notifier
    COMMAND dbus-run-session -- $<TARGET_FILE:qindaqt_freedesktop_feedback_notifier_tests>)
set_tests_properties(qindaqt.session-desktop-controls-feedback-notifier PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    TIMEOUT 45
    LABELS "session;session-desktop-controls;dbus")

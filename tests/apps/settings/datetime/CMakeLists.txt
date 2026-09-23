# SPDX-License-Identifier: GPL-3.0-or-later

# ADR-0211: the Date & time model over the seam's own fakes. No bus, no
# systemd, no polkit agent -- the environment below makes that provable
# rather than merely intended.
qt_add_executable(
    qindaqt_datetime_settings_model_tests
    tst_datetime_settings_model.cpp
)
target_link_libraries(
    qindaqt_datetime_settings_model_tests
    PRIVATE qindaqt_settings_datetime Qt6::Test
)
qindaqt_enable_warnings(qindaqt_datetime_settings_model_tests)
add_test(NAME qindaqt.settings-datetime-model
         COMMAND qindaqt_datetime_settings_model_tests)
set_tests_properties(qindaqt.settings-datetime-model PROPERTIES
    ENVIRONMENT "DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent;DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent"
    LABELS "settings;datetime;model")

# The production adapter is exercised over the public SettingsTransport seam;
# the injected fake never opens a real session bus or edits user preferences.
qt_add_executable(qindaqt_week_start_preference_tests
                  tst_week_start_preference.cpp)
target_include_directories(qindaqt_week_start_preference_tests PRIVATE
                           "${PROJECT_SOURCE_DIR}/src/apps/settings/datetime")
target_link_libraries(qindaqt_week_start_preference_tests
                      PRIVATE qindaqt_settings_datetime Qt6::Test)
qindaqt_enable_warnings(qindaqt_week_start_preference_tests)
add_test(NAME qindaqt.settings-week-start-preference
         COMMAND qindaqt_week_start_preference_tests)
set_tests_properties(qindaqt.settings-week-start-preference PROPERTIES
    ENVIRONMENT "DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent;DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent"
    LABELS "settings;datetime;model")

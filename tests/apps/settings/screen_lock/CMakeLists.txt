# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_executable(qindaqt_screen_lock_settings_tests
    tst_screen_lock_settings.cpp)
target_link_libraries(qindaqt_screen_lock_settings_tests
    PRIVATE qindaqt_settings_screen_lock Qt6::Test)
qindaqt_enable_warnings(qindaqt_screen_lock_settings_tests)
add_test(NAME qindaqt.settings-screen-lock-model
    COMMAND qindaqt_screen_lock_settings_tests)
set_tests_properties(qindaqt.settings-screen-lock-model PROPERTIES
    LABELS "settings;screen-lock")

qt_add_executable(qindaqt_screen_lock_resume_settings_tests
    tst_screen_lock_resume_settings.cpp)
target_link_libraries(qindaqt_screen_lock_resume_settings_tests
    PRIVATE qindaqt_settings_screen_lock Qt6::Test)
qindaqt_enable_warnings(qindaqt_screen_lock_resume_settings_tests)
add_test(NAME qindaqt.settings-screen-lock-resume
    COMMAND qindaqt_screen_lock_resume_settings_tests)
set_tests_properties(qindaqt.settings-screen-lock-resume PROPERTIES
    LABELS "settings;screen-lock")

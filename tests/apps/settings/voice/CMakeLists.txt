# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_executable(qindaqt_settings_voice_tests tst_voice_settings.cpp)
target_link_libraries(qindaqt_settings_voice_tests PRIVATE
    qindaqt_settings_voice QindaQt::VoicePreferences
    QindaQt::SettingsClient QindaQt::VoiceClient Qt6::Test)
qindaqt_enable_warnings(qindaqt_settings_voice_tests)
add_test(NAME qindaqt.settings-voice COMMAND qindaqt_settings_voice_tests)
set_tests_properties(qindaqt.settings-voice PROPERTIES
    ENVIRONMENT "DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent;DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent"
    LABELS "settings;voice;model;policy" TIMEOUT 30)

if(TARGET qindaqt_voice_console_support)
    target_compile_definitions(qindaqt_settings_voice_tests
        PRIVATE QINDAQT_HAVE_VOICE_CONSOLE)
    target_link_libraries(qindaqt_settings_voice_tests
        PRIVATE qindaqt_voice_console_support)
endif()

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

qt_add_executable(qindaqt_settings_voice_credentials_tests tst_voice_credentials.cpp)
target_link_libraries(qindaqt_settings_voice_credentials_tests PRIVATE
    qindaqt_settings_voice qindaqt_settings_voice_qmlplugin QindaQt::ControlsQml
    QindaQt::TokensQml Qt6::Test Qt6::Quick Qt6::Qml)
target_compile_definitions(qindaqt_settings_voice_credentials_tests PRIVATE
    VOICE_TEST_QML_PATH="${CMAKE_BINARY_DIR}/qml"
    VOICE_SOURCE_QML_PATH="${CMAKE_SOURCE_DIR}/src/apps/settings/voice/qml/VoiceCredentialSection.qml")
qindaqt_enable_warnings(qindaqt_settings_voice_credentials_tests)
add_test(NAME qindaqt.settings-voice-credentials COMMAND qindaqt_settings_voice_credentials_tests)
set_tests_properties(qindaqt.settings-voice-credentials PROPERTIES TIMEOUT 30
    LABELS "settings;voice;credentials;qml"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent;DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent")

qt_add_executable(qindaqt_voice_configuration_tests tst_voice_configuration.cpp)
target_link_libraries(qindaqt_voice_configuration_tests PRIVATE QindaQt::VoiceConfiguration Qt6::Test)
qindaqt_enable_warnings(qindaqt_voice_configuration_tests)
add_test(NAME qindaqt.voice-configuration COMMAND qindaqt_voice_configuration_tests)
set_tests_properties(qindaqt.voice-configuration PROPERTIES TIMEOUT 30 LABELS "voice;configuration;pure"
    ENVIRONMENT "DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent;DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent")

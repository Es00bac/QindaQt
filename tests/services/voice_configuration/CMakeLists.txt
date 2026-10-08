qt_add_executable(qindaqt_voice_configuration_tests tst_voice_configuration.cpp)
target_link_libraries(qindaqt_voice_configuration_tests PRIVATE QindaQt::VoiceConfiguration Qt6::Test)
qindaqt_enable_warnings(qindaqt_voice_configuration_tests)
add_test(NAME qindaqt.voice-configuration COMMAND qindaqt_voice_configuration_tests)
set_tests_properties(qindaqt.voice-configuration PROPERTIES TIMEOUT 30 LABELS "voice;configuration;pure"
    ENVIRONMENT "DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent;DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent")

find_program(QINDAQT_VOICE_DBUS_DAEMON dbus-daemon REQUIRED)
qt_add_executable(qindaqt_voice_configuration_transport_tests tst_voice_configuration_transport.cpp)
target_link_libraries(qindaqt_voice_configuration_transport_tests PRIVATE
    QindaQt::VoiceConfiguration Qt6::Test Qt6::DBus PkgConfig::QINDAQT_VOICE_DBUS)
target_compile_definitions(qindaqt_voice_configuration_transport_tests PRIVATE
    QINDAQT_DBUS_DAEMON_EXECUTABLE="${QINDAQT_VOICE_DBUS_DAEMON}")
qindaqt_enable_warnings(qindaqt_voice_configuration_transport_tests)
add_test(NAME qindaqt.voice-configuration-transport COMMAND qindaqt_voice_configuration_transport_tests)
set_tests_properties(qindaqt.voice-configuration-transport PROPERTIES TIMEOUT 30
    LABELS "voice;configuration;private-bus"
    ENVIRONMENT "DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent;DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent")

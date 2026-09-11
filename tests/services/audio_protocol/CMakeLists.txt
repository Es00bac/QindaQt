# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_executable(qindaqt_audio_protocol_tests tst_audio_protocol.cpp)
target_link_libraries(
    qindaqt_audio_protocol_tests PRIVATE QindaQt::AudioProtocol Qt6::DBus Qt6::Test
)
qindaqt_enable_warnings(qindaqt_audio_protocol_tests)
add_test(NAME qindaqt.audio-protocol COMMAND qindaqt_audio_protocol_tests)
set_tests_properties(qindaqt.audio-protocol PROPERTIES TIMEOUT 20 LABELS "service;audio")

find_program(QINDAQT_PROTOCOL_DBUS_DAEMON NAMES dbus-daemon REQUIRED)
target_compile_definitions(
    qindaqt_audio_protocol_tests
    PRIVATE QINDAQT_DBUS_DAEMON_EXECUTABLE="${QINDAQT_PROTOCOL_DBUS_DAEMON}"
)


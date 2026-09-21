# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_executable(qindaqt_voice_protocol_tests tst_voice_protocol.cpp)
target_link_libraries(qindaqt_voice_protocol_tests PRIVATE QindaQt::VoiceProtocol Qt6::Test)
qindaqt_enable_warnings(qindaqt_voice_protocol_tests)
add_test(NAME qindaqt.voice-protocol COMMAND qindaqt_voice_protocol_tests)
set_tests_properties(qindaqt.voice-protocol
                     PROPERTIES LABELS "service;voice;protocol" TIMEOUT 20)

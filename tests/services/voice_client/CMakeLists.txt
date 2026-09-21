# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_executable(qindaqt_voice_client_tests
                  tst_voice_client.cpp support/fake_voice_transport.h)
target_link_libraries(qindaqt_voice_client_tests PRIVATE QindaQt::VoiceClient Qt6::Test)
qindaqt_enable_warnings(qindaqt_voice_client_tests)
add_test(NAME qindaqt.voice-client COMMAND qindaqt_voice_client_tests)
set_tests_properties(qindaqt.voice-client
                     PROPERTIES LABELS "service;voice;client" TIMEOUT 30)

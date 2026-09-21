# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_executable(qindaqt_voice_applet_tests tst_voice_applet.cpp)
target_link_libraries(
    qindaqt_voice_applet_tests
    PRIVATE QindaQt::ShellVoiceAppletRuntime Qt6::Test
)
qindaqt_enable_warnings(qindaqt_voice_applet_tests)
add_test(NAME qindaqt.voice-applet COMMAND qindaqt_voice_applet_tests)
set_tests_properties(qindaqt.voice-applet
                     PROPERTIES LABELS "shell;voice;applet" TIMEOUT 30)

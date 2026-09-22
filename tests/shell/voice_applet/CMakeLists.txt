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

# AGENT-GUARD: these rows construct a QGuiApplication, so they must name a
# platform. Without this they default to xcb and abort before their first
# assertion on any machine with no display -- a headless runner, or a
# developer on a VT -- which reads as a real failure and is not one. Every
# one of them passes under offscreen. Keep this list in step with the rows
# above; a new GUI row added without it fails the same way.
set_tests_properties(
    qindaqt.voice-applet
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

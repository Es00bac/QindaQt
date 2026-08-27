# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_executable(
    qindaqt_audio_client_tests
    tst_audio_client.cpp
    support/fake_audio_transport.h
)
target_link_libraries(
    qindaqt_audio_client_tests PRIVATE QindaQt::AudioClient Qt6::DBus Qt6::Test
)
qindaqt_enable_warnings(qindaqt_audio_client_tests)
add_test(NAME qindaqt.audio-client COMMAND qindaqt_audio_client_tests)
set_tests_properties(qindaqt.audio-client PROPERTIES TIMEOUT 20 LABELS "service;audio")

qt_add_executable(
    qindaqt_audio_qt_transport_tests
    tst_qt_audio_transport.cpp
    ../audio_service/support/fake_audio_backend.h
)
target_link_libraries(
    qindaqt_audio_qt_transport_tests
    PRIVATE
        QindaQt::AudioClient
        QindaQt::AudioService
        Qt6::Core
        Qt6::DBus
        Qt6::Test
)
qindaqt_enable_warnings(qindaqt_audio_qt_transport_tests)
add_test(NAME qindaqt.audio-qt-transport COMMAND qindaqt_audio_qt_transport_tests)
set_tests_properties(
    qindaqt.audio-qt-transport PROPERTIES TIMEOUT 20 LABELS "service;audio;dbus"
)

find_program(QINDAQT_DBUS_DAEMON_EXECUTABLE NAMES dbus-daemon REQUIRED)
qt_add_executable(qindaqt_audio_activation_tests tst_audio_activation.cpp)
target_link_libraries(
    qindaqt_audio_activation_tests
    PRIVATE QindaQt::AudioClient Qt6::DBus Qt6::Test
)
target_compile_definitions(
    qindaqt_audio_activation_tests
    PRIVATE
        QINDAQT_AUDIO_SERVICE_EXECUTABLE="$<TARGET_FILE:qindaqt-audio-service>"
        QINDAQT_DBUS_DAEMON_EXECUTABLE="${QINDAQT_DBUS_DAEMON_EXECUTABLE}"
)
qindaqt_enable_warnings(qindaqt_audio_activation_tests)
add_dependencies(qindaqt_audio_activation_tests qindaqt-audio-service)
add_test(NAME qindaqt.audio-activation COMMAND qindaqt_audio_activation_tests)
set_tests_properties(
    qindaqt.audio-activation
    PROPERTIES LABELS "service;audio;dbus;activation" TIMEOUT 20 RUN_SERIAL TRUE
)

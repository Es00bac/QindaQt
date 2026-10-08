# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_executable(qindaqt_audio_protocol_tests tst_audio_protocol.cpp)
target_link_libraries(
    qindaqt_audio_protocol_tests PRIVATE QindaQt::AudioProtocol Qt6::DBus Qt6::Test
)
qindaqt_enable_warnings(qindaqt_audio_protocol_tests)
add_test(NAME qindaqt.audio-protocol COMMAND qindaqt_audio_protocol_tests)
set_tests_properties(qindaqt.audio-protocol PROPERTIES TIMEOUT 20 LABELS "service;audio")

# The shared gain law (ADR-0171). Split from the wire row by failure mode: it
# is pure arithmetic with no bus, no daemon and no timeout risk.
qt_add_executable(qindaqt_audio_gain_tests tst_audio_gain.cpp)
target_link_libraries(
    qindaqt_audio_gain_tests PRIVATE QindaQt::AudioProtocol Qt6::Test
)
qindaqt_enable_warnings(qindaqt_audio_gain_tests)
add_test(NAME qindaqt.audio-gain COMMAND qindaqt_audio_gain_tests)
set_tests_properties(qindaqt.audio-gain PROPERTIES TIMEOUT 20 LABELS "service;audio")

find_program(QINDAQT_PROTOCOL_DBUS_DAEMON NAMES dbus-daemon REQUIRED)
target_compile_definitions(
    qindaqt_audio_protocol_tests
    PRIVATE QINDAQT_DBUS_DAEMON_EXECUTABLE="${QINDAQT_PROTOCOL_DBUS_DAEMON}"
)


# Actual FILE_SET stage, separate linked client and mandatory negative/restore.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME qindaqt.audio-installed-sdk-closure
    COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/run_installed_audio_sdk.py"
        --source-root "${CMAKE_SOURCE_DIR}"
        --output "${CMAKE_CURRENT_BINARY_DIR}/installed-sdk-evidence"
        --cmake "${CMAKE_COMMAND}")
set_tests_properties(qindaqt.audio-installed-sdk-closure
    PROPERTIES TIMEOUT 240 LABELS "service;audio;installed;sdk")

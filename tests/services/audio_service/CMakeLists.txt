# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_executable(
    qindaqt_audio_service_tests
    tst_audio_service.cpp
    support/fake_audio_backend.h
)
target_link_libraries(
    qindaqt_audio_service_tests PRIVATE QindaQt::AudioService Qt6::DBus Qt6::Test
)
qindaqt_enable_warnings(qindaqt_audio_service_tests)
add_test(NAME qindaqt.audio-service COMMAND qindaqt_audio_service_tests)
set_tests_properties(qindaqt.audio-service PROPERTIES LABELS "service;audio")

find_program(QINDAQT_PIPEWIRE_EXECUTABLE NAMES pipewire REQUIRED)
find_program(QINDAQT_WIREPLUMBER_EXECUTABLE NAMES wireplumber REQUIRED)
find_program(QINDAQT_PW_CLI_EXECUTABLE NAMES pw-cli REQUIRED)
find_program(QINDAQT_PW_CAT_EXECUTABLE NAMES pw-cat REQUIRED)
find_file(
    QINDAQT_PIPEWIRE_TEST_CONFIG
    NAMES pipewire.conf
    PATHS /usr/share /usr/local/share
    PATH_SUFFIXES pipewire
    REQUIRED
)

qt_add_executable(qindaqt_audio_wireplumber_runtime_tests tst_wireplumber_runtime.cpp)
target_link_libraries(
    qindaqt_audio_wireplumber_runtime_tests
    PRIVATE QindaQt::AudioService Qt6::Test
)
target_compile_definitions(
    qindaqt_audio_wireplumber_runtime_tests
    PRIVATE
        QINDAQT_PIPEWIRE_EXECUTABLE="${QINDAQT_PIPEWIRE_EXECUTABLE}"
        QINDAQT_WIREPLUMBER_EXECUTABLE="${QINDAQT_WIREPLUMBER_EXECUTABLE}"
        QINDAQT_PW_CLI_EXECUTABLE="${QINDAQT_PW_CLI_EXECUTABLE}"
        QINDAQT_PW_CAT_EXECUTABLE="${QINDAQT_PW_CAT_EXECUTABLE}"
        QINDAQT_PIPEWIRE_TEST_CONFIG="${QINDAQT_PIPEWIRE_TEST_CONFIG}"
)
qindaqt_enable_warnings(qindaqt_audio_wireplumber_runtime_tests)
add_test(
    NAME qindaqt.audio-wireplumber-runtime
    COMMAND qindaqt_audio_wireplumber_runtime_tests
)
set_tests_properties(
    qindaqt.audio-wireplumber-runtime
    PROPERTIES
        LABELS "service;audio;wireplumber;isolated-runtime"
        TIMEOUT 45
        RUN_SERIAL TRUE
)

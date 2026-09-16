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

# The console's own state (ADR-0173). Split from the service row by failure
# mode: pure model logic with no backend, no bus and no daemon.
qt_add_executable(qindaqt_audio_console_model_tests tst_console_model.cpp)
target_link_libraries(
    qindaqt_audio_console_model_tests PRIVATE QindaQt::AudioService Qt6::Test
)
qindaqt_enable_warnings(qindaqt_audio_console_model_tests)
add_test(NAME qindaqt.audio-console-model COMMAND qindaqt_audio_console_model_tests)
set_tests_properties(qindaqt.audio-console-model PROPERTIES LABELS "service;audio")

# The loopback argument shape (ADR-0173). Pure string construction, so it needs
# no daemon; the point is that a malformed module argument is invisible at
# runtime and must be caught here.
qt_add_executable(qindaqt_audio_routing_tests tst_wireplumber_routing.cpp)
target_link_libraries(
    qindaqt_audio_routing_tests PRIVATE QindaQt::AudioService Qt6::Test
)
qindaqt_enable_warnings(qindaqt_audio_routing_tests)
add_test(NAME qindaqt.audio-routing COMMAND qindaqt_audio_routing_tests)
set_tests_properties(qindaqt.audio-routing PROPERTIES LABELS "service;audio")

# AGENT-GUARD: This defaults ON for broad build jobs. The pinned native plugin
# lane disables only these process-launching rows because Gentoo's official
# binhost has no PipeWire `extra` variant providing pw-cat.
if(NOT QINDAQT_ENABLE_AUDIO_LIVE_RUNTIME_TESTS)
    return()
endif()

find_program(QINDAQT_PIPEWIRE_EXECUTABLE NAMES pipewire REQUIRED)
find_program(QINDAQT_WIREPLUMBER_EXECUTABLE NAMES wireplumber REQUIRED)
find_program(QINDAQT_PW_CLI_EXECUTABLE NAMES pw-cli REQUIRED)
find_program(QINDAQT_PW_CAT_EXECUTABLE NAMES pw-cat REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
pkg_check_modules(WirePlumberTest REQUIRED IMPORTED_TARGET wireplumber-0.5>=0.5)
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
        TIMEOUT 120
        RUN_SERIAL TRUE
)

qt_add_executable(
    qindaqt_audio_wireplumber_reset_lifecycle_tests
    tst_wireplumber_reset_lifecycle.cpp
)
target_include_directories(
    qindaqt_audio_wireplumber_reset_lifecycle_tests
    PRIVATE ${CMAKE_SOURCE_DIR}/src/services/audio_service/src
)
target_link_libraries(
    qindaqt_audio_wireplumber_reset_lifecycle_tests
    PRIVATE
        QindaQt::AudioService
        PkgConfig::WirePlumberTest
        Threads::Threads
        Qt6::Test
)
target_compile_definitions(
    qindaqt_audio_wireplumber_reset_lifecycle_tests
    PRIVATE
        QINDAQT_PIPEWIRE_EXECUTABLE="${QINDAQT_PIPEWIRE_EXECUTABLE}"
        QINDAQT_PIPEWIRE_TEST_CONFIG="${QINDAQT_PIPEWIRE_TEST_CONFIG}"
        QT_NO_KEYWORDS
)
qindaqt_enable_warnings(qindaqt_audio_wireplumber_reset_lifecycle_tests)
add_test(
    NAME qindaqt.audio-wireplumber-reset-lifecycle
    COMMAND qindaqt_audio_wireplumber_reset_lifecycle_tests
)
set_tests_properties(
    qindaqt.audio-wireplumber-reset-lifecycle
    PROPERTIES
        LABELS "service;audio;wireplumber;isolated-runtime"
        TIMEOUT 45
        RUN_SERIAL TRUE
)

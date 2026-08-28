# SPDX-License-Identifier: GPL-3.0-or-later

# AGENT-NOTE: Standalone test seam for the Audio applet A1 slice, mirroring
# the tests/shell convention of compiling the owned library sources directly
# into each test target. It is deliberately not referenced by
# tests/shell/CMakeLists.txt; the manager wires it in as an additive
# `add_subdirectory(audio_applet)` when the candidate integrates.
find_package(Qt6 6.11 REQUIRED COMPONENTS Core Test)

set(QINDAQT_AUDIO_APPLET_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/../../../src/shell/audio_applet/audio_applet_model.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../../../src/shell/audio_applet/audio_applet_controller.cpp"
)

qt_add_executable(qindaqt_audio_applet_model_tests tst_audio_applet_model.cpp)
target_sources(
    qindaqt_audio_applet_model_tests
    PRIVATE "${QINDAQT_AUDIO_APPLET_SOURCES}"
)
target_include_directories(
    qindaqt_audio_applet_model_tests
    PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../../../src/shell/audio_applet"
)
target_link_libraries(
    qindaqt_audio_applet_model_tests
    PRIVATE QindaQt::AudioClient Qt6::Core Qt6::Test
)
qindaqt_enable_warnings(qindaqt_audio_applet_model_tests)
add_test(
    NAME qindaqt.audio-applet-model
    COMMAND qindaqt_audio_applet_model_tests
)

qt_add_executable(
    qindaqt_audio_applet_controller_tests
    tst_audio_applet_controller.cpp
    "${QINDAQT_AUDIO_APPLET_SOURCES}"
)
target_include_directories(
    qindaqt_audio_applet_controller_tests
    PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../../../src/shell/audio_applet"
)
target_link_libraries(
    qindaqt_audio_applet_controller_tests
    PRIVATE QindaQt::AudioClient Qt6::Core Qt6::Test
)
qindaqt_enable_warnings(qindaqt_audio_applet_controller_tests)
add_test(
    NAME qindaqt.audio-applet-controller
    COMMAND qindaqt_audio_applet_controller_tests
)

# SPDX-License-Identifier: GPL-3.0-or-later

# The Streaming route model over fake ports: no OBS, no keyring, no bus.
qt_add_executable(qindaqt_streaming_settings_model_tests
    tst_streaming_settings_model.cpp)
target_link_libraries(qindaqt_streaming_settings_model_tests
    PRIVATE qindaqt_settings_streaming Qt6::Test)
qindaqt_enable_warnings(qindaqt_streaming_settings_model_tests)
add_test(NAME qindaqt.settings-streaming-model
    COMMAND qindaqt_streaming_settings_model_tests)
set_tests_properties(qindaqt.settings-streaming-model PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    LABELS "settings;streaming;model")

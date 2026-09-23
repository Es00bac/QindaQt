# SPDX-License-Identifier: GPL-3.0-or-later

# The Streaming route model over fake ports: no OBS, no keyring, no bus.
qt_add_executable(qindaqt_streaming_settings_model_tests
    tst_streaming_settings_model.cpp)
target_compile_definitions(qindaqt_streaming_settings_model_tests PRIVATE
    QINDAQT_OBS_LOGIN_ENTRY="${PROJECT_SOURCE_DIR}/src/services/streaming_preferences/data/qindaqt-obs-login.desktop")
target_link_libraries(qindaqt_streaming_settings_model_tests
    PRIVATE qindaqt_settings_streaming Qt6::Test)
qindaqt_enable_warnings(qindaqt_streaming_settings_model_tests)
add_test(NAME qindaqt.settings-streaming-model
    COMMAND qindaqt_streaming_settings_model_tests)
set_tests_properties(qindaqt.settings-streaming-model PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    LABELS "settings;streaming;model")

qt_add_executable(qindaqt_settings_streaming_page_test tst_streaming_page.cpp)
target_compile_definitions(qindaqt_settings_streaming_page_test PRIVATE
    QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
    QINDAQT_QML_IMPORT_PATH="${CMAKE_BINARY_DIR}/qml"
    QINDAQT_STREAMING_PAGE_QML_PATH="${PROJECT_SOURCE_DIR}/src/apps/settings/streaming/qml/StreamingPage.qml")
target_link_libraries(qindaqt_settings_streaming_page_test PRIVATE
    qindaqt_settings_streaming qindaqt_settings_appearance
    QindaQt::TokensQml qindaqt_themes Qt6::Quick Qt6::QuickControls2 Qt6::Test)
add_dependencies(qindaqt_settings_streaming_page_test
    qindaqt_tokens_qmlplugin qindaqt_controls_qmlplugin
    qindaqt_settings_streaming_qmlplugin)
qindaqt_enable_warnings(qindaqt_settings_streaming_page_test)
add_test(NAME qindaqt.settings-streaming-page COMMAND qindaqt_settings_streaming_page_test)
set_tests_properties(qindaqt.settings-streaming-page PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_LINUX_ACCESSIBILITY_ALWAYS_ON=1;QT_FATAL_WARNINGS=1;DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent;DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent"
    LABELS "offscreen;settings;streaming;accessibility")

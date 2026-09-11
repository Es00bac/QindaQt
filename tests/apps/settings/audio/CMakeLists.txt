# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_executable(
    qindaqt_settings_audio_model_tests
    tst_audio_settings_model.cpp
    audio_settings_test_support.h
)
target_include_directories(
    qindaqt_settings_audio_model_tests PRIVATE "${PROJECT_SOURCE_DIR}"
)
target_link_libraries(
    qindaqt_settings_audio_model_tests
    PRIVATE qindaqt_settings_audio Qt6::Test
)
qindaqt_enable_warnings(qindaqt_settings_audio_model_tests)
add_test(
    NAME qindaqt.settings-audio-model
    COMMAND qindaqt_settings_audio_model_tests
)

qt_add_executable(
    qindaqt_settings_audio_model_adversarial_tests
    tst_audio_settings_model_adversarial.cpp
    audio_settings_test_support.h
)
target_include_directories(
    qindaqt_settings_audio_model_adversarial_tests PRIVATE "${PROJECT_SOURCE_DIR}"
)
target_link_libraries(
    qindaqt_settings_audio_model_adversarial_tests
    PRIVATE qindaqt_settings_audio Qt6::Test
)
qindaqt_enable_warnings(qindaqt_settings_audio_model_adversarial_tests)
add_test(
    NAME qindaqt.settings-audio-model-adversarial
    COMMAND qindaqt_settings_audio_model_adversarial_tests
)

qt_add_executable(
    qindaqt_settings_audio_page_tests
    tst_audio_page.cpp
    stub_audio_settings_model.h
)
target_include_directories(
    qindaqt_settings_audio_page_tests PRIVATE "${PROJECT_SOURCE_DIR}"
)
target_compile_definitions(
    qindaqt_settings_audio_page_tests PRIVATE
    QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
    QINDAQT_QML_IMPORT_PATH="${CMAKE_BINARY_DIR}/qml"
    QINDAQT_AUDIO_PAGE_QML_PATH="${PROJECT_SOURCE_DIR}/src/apps/settings/audio/qml/AudioPage.qml"
)
target_link_libraries(
    qindaqt_settings_audio_page_tests
    PRIVATE qindaqt_settings_audio qindaqt_settings_appearance
            QindaQt::TokensQml qindaqt_themes
            QindaQt::ShellIcons qindaqt_shell_iconsplugin
            Qt6::Quick Qt6::QuickControls2 Qt6::Test
)
qindaqt_enable_warnings(qindaqt_settings_audio_page_tests)
add_dependencies(
    qindaqt_settings_audio_page_tests
    qindaqt_tokens_qmlplugin
    qindaqt_controls_qmlplugin
    qindaqt_settings_audio_qmlplugin
    qindaqt_shell_iconsplugin
)
add_test(NAME qindaqt.settings-audio-page COMMAND qindaqt_settings_audio_page_tests)
set_tests_properties(
    qindaqt.settings-audio-page PROPERTIES
    LABELS "offscreen;settings;audio;accessibility"
    ENVIRONMENT
        "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_LINUX_ACCESSIBILITY_ALWAYS_ON=1"
)

add_test(
    NAME qindaqt.settings-audio-boundary
    COMMAND "${CMAKE_COMMAND}" -DSOURCE_ROOT=${PROJECT_SOURCE_DIR}
            -P "${CMAKE_CURRENT_SOURCE_DIR}/check_boundary.cmake"
)
set_tests_properties(
    qindaqt.settings-audio-boundary PROPERTIES
    LABELS "settings;audio;boundary"
)

add_test(
    NAME qindaqt.settings-audio-boundary-poison
    COMMAND "${CMAKE_COMMAND}"
            -DBINARY_ROOT=${CMAKE_BINARY_DIR}
            -DPOISON_ROOT=${CMAKE_CURRENT_BINARY_DIR}/boundary-poison
            -DCHECK_SCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/check_boundary.cmake
            -P "${CMAKE_CURRENT_SOURCE_DIR}/check_boundary_negative.cmake"
)
set_tests_properties(
    qindaqt.settings-audio-boundary-poison PROPERTIES
    LABELS "settings;audio;boundary;poison"
)

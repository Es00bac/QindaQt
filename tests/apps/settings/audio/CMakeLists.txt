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
    qindaqt_settings_audio_stream_routing_tests
    tst_audio_stream_routing.cpp
    audio_settings_test_support.h
)
target_include_directories(
    qindaqt_settings_audio_stream_routing_tests PRIVATE "${PROJECT_SOURCE_DIR}"
)
target_link_libraries(
    qindaqt_settings_audio_stream_routing_tests
    PRIVATE qindaqt_settings_audio Qt6::Test
)
qindaqt_enable_warnings(qindaqt_settings_audio_stream_routing_tests)
add_test(
    NAME qindaqt.settings-audio-stream-routing
    COMMAND qindaqt_settings_audio_stream_routing_tests
)

qt_add_executable(
    qindaqt_settings_audio_stream_routing_page_tests
    tst_audio_stream_routing_page.cpp
    audio_page_test_support.h
    stub_audio_settings_model.h
)
target_include_directories(
    qindaqt_settings_audio_stream_routing_page_tests PRIVATE
    "${PROJECT_SOURCE_DIR}"
)
target_compile_definitions(
    qindaqt_settings_audio_stream_routing_page_tests PRIVATE
    QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
    QINDAQT_QML_IMPORT_PATH="${CMAKE_BINARY_DIR}/qml"
    QINDAQT_AUDIO_PAGE_QML_PATH="${PROJECT_SOURCE_DIR}/src/apps/settings/audio/qml/AudioPage.qml"
)
target_link_libraries(
    qindaqt_settings_audio_stream_routing_page_tests
    PRIVATE qindaqt_settings_audio qindaqt_settings_appearance
            QindaQt::TokensQml qindaqt_themes
            QindaQt::ShellIcons qindaqt_shell_iconsplugin
            Qt6::Quick Qt6::QuickControls2 Qt6::Test
)
qindaqt_enable_warnings(qindaqt_settings_audio_stream_routing_page_tests)
add_dependencies(
    qindaqt_settings_audio_stream_routing_page_tests
    qindaqt_tokens_qmlplugin
    qindaqt_controls_qmlplugin
    qindaqt_settings_audio_qmlplugin
    qindaqt_shell_iconsplugin
)
add_test(NAME qindaqt.settings-audio-stream-routing-page
         COMMAND qindaqt_settings_audio_stream_routing_page_tests)
set_tests_properties(
    qindaqt.settings-audio-stream-routing-page PROPERTIES
    LABELS "offscreen;settings;audio;accessibility"
    ENVIRONMENT
        "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_LINUX_ACCESSIBILITY_ALWAYS_ON=1"
)

qt_add_executable(
    qindaqt_settings_audio_page_tests
    tst_audio_page.cpp
    audio_page_test_support.h
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
# The capture probe (O6). Not an add_test: it produces review artifacts, not a
# pass/fail row, and the console polish is judged by a human looking at two
# pictures.
# AGENT-NOTE: the stub header declares a Q_OBJECT, so it must be listed as a
# source for AUTOMOC to generate its meta-object — exactly as the page test
# above does. Omitting it links with an undefined vtable.
qt_add_executable(
    qindaqt_settings_audio_visual_probe
    visual_probe.cpp
    stub_audio_settings_model.h
)
target_include_directories(
    qindaqt_settings_audio_visual_probe PRIVATE "${PROJECT_SOURCE_DIR}"
                                                "${CMAKE_CURRENT_SOURCE_DIR}"
)
target_compile_definitions(
    qindaqt_settings_audio_visual_probe PRIVATE
    QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
    QINDAQT_QML_IMPORT_PATH="${CMAKE_BINARY_DIR}/qml"
    QINDAQT_AUDIO_PAGE_QML_PATH="${PROJECT_SOURCE_DIR}/src/apps/settings/audio/qml/AudioPage.qml"
)
target_link_libraries(
    qindaqt_settings_audio_visual_probe
    PRIVATE qindaqt_settings_audio qindaqt_settings_appearance
            QindaQt::TokensQml qindaqt_themes
            QindaQt::ShellIcons qindaqt_shell_iconsplugin
            Qt6::Quick Qt6::QuickControls2 Qt6::Test
)
qindaqt_enable_warnings(qindaqt_settings_audio_visual_probe)
add_dependencies(
    qindaqt_settings_audio_visual_probe
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

# The console grid's assertions (band alignment, the one gain law, control
# dispatch) are their own executable: the route's page suite crossed the
# repository's 600-line source ceiling when they landed, and the two suites
# fail for different reasons. Same harness (audio_page_test_support.h), same
# environment.
qt_add_executable(
    qindaqt_settings_audio_console_page_tests
    tst_audio_console_page.cpp
    audio_page_test_support.h
    stub_audio_settings_model.h
)
target_include_directories(
    qindaqt_settings_audio_console_page_tests PRIVATE "${PROJECT_SOURCE_DIR}"
)
target_compile_definitions(
    qindaqt_settings_audio_console_page_tests PRIVATE
    QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
    QINDAQT_QML_IMPORT_PATH="${CMAKE_BINARY_DIR}/qml"
    QINDAQT_AUDIO_PAGE_QML_PATH="${PROJECT_SOURCE_DIR}/src/apps/settings/audio/qml/AudioPage.qml"
)
target_link_libraries(
    qindaqt_settings_audio_console_page_tests
    PRIVATE qindaqt_settings_audio qindaqt_settings_appearance
            QindaQt::TokensQml qindaqt_themes
            QindaQt::ShellIcons qindaqt_shell_iconsplugin
            Qt6::Quick Qt6::QuickControls2 Qt6::Test
)
qindaqt_enable_warnings(qindaqt_settings_audio_console_page_tests)
add_dependencies(
    qindaqt_settings_audio_console_page_tests
    qindaqt_tokens_qmlplugin
    qindaqt_controls_qmlplugin
    qindaqt_settings_audio_qmlplugin
    qindaqt_shell_iconsplugin
)
add_test(
    NAME qindaqt.settings-audio-console-page
    COMMAND qindaqt_settings_audio_console_page_tests
)
set_tests_properties(
    qindaqt.settings-audio-console-page PROPERTIES
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

# A11 wheel delivery stays separate from the route/page tests while Network
# Audio changes the page tests in another isolated lane.
qt_add_executable(
    qindaqt_settings_audio_wheel_tests
    tst_audio_wheel.cpp
    audio_page_test_support.h
    stub_audio_settings_model.h
)
target_include_directories(
    qindaqt_settings_audio_wheel_tests PRIVATE "${PROJECT_SOURCE_DIR}"
)
target_compile_definitions(
    qindaqt_settings_audio_wheel_tests PRIVATE
    QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
    QINDAQT_QML_IMPORT_PATH="${CMAKE_BINARY_DIR}/qml"
    QINDAQT_AUDIO_PAGE_QML_PATH="${PROJECT_SOURCE_DIR}/src/apps/settings/audio/qml/AudioPage.qml"
)
target_link_libraries(
    qindaqt_settings_audio_wheel_tests
    PRIVATE qindaqt_settings_audio qindaqt_settings_appearance
            QindaQt::TokensQml qindaqt_themes
            QindaQt::ShellIcons qindaqt_shell_iconsplugin
            Qt6::Quick Qt6::QuickControls2 Qt6::Test
)
qindaqt_enable_warnings(qindaqt_settings_audio_wheel_tests)
add_dependencies(
    qindaqt_settings_audio_wheel_tests
    qindaqt_tokens_qmlplugin
    qindaqt_controls_qmlplugin
    qindaqt_settings_audio_qmlplugin
    qindaqt_shell_iconsplugin
)
add_test(NAME qindaqt.settings-audio-wheel
         COMMAND qindaqt_settings_audio_wheel_tests)
set_tests_properties(
    qindaqt.settings-audio-wheel PROPERTIES
    LABELS "offscreen;settings;audio;wheel"
    ENVIRONMENT
        "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_LINUX_ACCESSIBILITY_ALWAYS_ON=1"
)

# Settings-owned connection-code contract and compact/wide guided form.
qt_add_executable(
    qindaqt_settings_audio_peer_code_tests
    tst_audio_peer_code.cpp
    audio_settings_test_support.h
)
target_include_directories(
    qindaqt_settings_audio_peer_code_tests PRIVATE "${PROJECT_SOURCE_DIR}"
)
target_link_libraries(
    qindaqt_settings_audio_peer_code_tests
    PRIVATE qindaqt_settings_audio Qt6::Test
)
qindaqt_enable_warnings(qindaqt_settings_audio_peer_code_tests)
add_test(NAME qindaqt.settings-audio-peer-code
         COMMAND qindaqt_settings_audio_peer_code_tests)

qt_add_executable(
    qindaqt_settings_audio_peer_code_page_tests
    tst_audio_peer_code_page.cpp
    audio_page_test_support.h
    stub_audio_settings_model.h
)
target_include_directories(
    qindaqt_settings_audio_peer_code_page_tests PRIVATE "${PROJECT_SOURCE_DIR}"
)
target_compile_definitions(
    qindaqt_settings_audio_peer_code_page_tests PRIVATE
    QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
    QINDAQT_QML_IMPORT_PATH="${CMAKE_BINARY_DIR}/qml"
    QINDAQT_AUDIO_PAGE_QML_PATH="${PROJECT_SOURCE_DIR}/src/apps/settings/audio/qml/AudioPage.qml"
)
target_link_libraries(
    qindaqt_settings_audio_peer_code_page_tests
    PRIVATE qindaqt_settings_audio qindaqt_settings_appearance
            QindaQt::TokensQml qindaqt_themes
            QindaQt::ShellIcons qindaqt_shell_iconsplugin
            Qt6::Quick Qt6::QuickControls2 Qt6::Test
)
qindaqt_enable_warnings(qindaqt_settings_audio_peer_code_page_tests)
add_dependencies(
    qindaqt_settings_audio_peer_code_page_tests
    qindaqt_tokens_qmlplugin
    qindaqt_controls_qmlplugin
    qindaqt_settings_audio_qmlplugin
    qindaqt_shell_iconsplugin
)
add_test(NAME qindaqt.settings-audio-peer-code-page
         COMMAND qindaqt_settings_audio_peer_code_page_tests)
set_tests_properties(
    qindaqt.settings-audio-peer-code-page PROPERTIES
    LABELS "offscreen;settings;audio;accessibility"
    ENVIRONMENT
        "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_LINUX_ACCESSIBILITY_ALWAYS_ON=1"
)

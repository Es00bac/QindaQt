# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_executable(qindaqt_display_model_tests
                  tst_display_settings_model.cpp
                  ${PROJECT_SOURCE_DIR}/tests/services/display_client/support/fake_display_transport.h)
target_include_directories(qindaqt_display_model_tests PRIVATE ${PROJECT_SOURCE_DIR})
target_compile_definitions(
    qindaqt_display_model_tests PRIVATE
    QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)
target_link_libraries(
    qindaqt_display_model_tests
    PRIVATE qindaqt_settings_display Qt6::Test
)
qindaqt_enable_warnings(qindaqt_display_model_tests)
add_test(NAME qindaqt.display-settings-model
         COMMAND qindaqt_display_model_tests)
set_tests_properties(qindaqt.display-settings-model PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_LINUX_ACCESSIBILITY_ALWAYS_ON=1")

qt_add_executable(qindaqt_display_model_adversarial_tests
                  tst_display_settings_model_adversarial.cpp
                  ${PROJECT_SOURCE_DIR}/tests/services/display_client/support/fake_display_transport.h)
target_include_directories(qindaqt_display_model_adversarial_tests PRIVATE ${PROJECT_SOURCE_DIR})
target_compile_definitions(
    qindaqt_display_model_adversarial_tests PRIVATE
    QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)
target_link_libraries(
    qindaqt_display_model_adversarial_tests
    PRIVATE qindaqt_settings_display Qt6::Test
)
qindaqt_enable_warnings(qindaqt_display_model_adversarial_tests)
add_test(NAME qindaqt.display-settings-model-adversarial
         COMMAND qindaqt_display_model_adversarial_tests)
set_tests_properties(qindaqt.display-settings-model-adversarial PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_LINUX_ACCESSIBILITY_ALWAYS_ON=1")

qt_add_executable(qindaqt_display_page_tests
                  tst_display_page.cpp
                  display_page_test_support.h
                  stub_display_model.h)
target_include_directories(qindaqt_display_page_tests PRIVATE ${PROJECT_SOURCE_DIR})
target_compile_definitions(
    qindaqt_display_page_tests PRIVATE
    QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
    QINDAQT_QML_IMPORT_PATH="${CMAKE_BINARY_DIR}/qml"
    QINDAQT_DISPLAY_PAGE_QML_PATH="${PROJECT_SOURCE_DIR}/src/apps/settings/display/qml/DisplayPage.qml"
)
# The page now hosts the night light section, which imports
# QindaQt.Shell.Icons; a static module resolves only when its plugin is
# linked into the test binary (settings_center precedent).
target_link_libraries(
    qindaqt_display_page_tests
    PRIVATE qindaqt_settings_display qindaqt_settings_appearance QindaQt::TokensQml
            QindaQt::ShellIcons qindaqt_shell_iconsplugin
            Qt6::Quick Qt6::QuickControls2 Qt6::Test
)
qindaqt_enable_warnings(qindaqt_display_page_tests)

add_dependencies(
    qindaqt_display_page_tests
    qindaqt_tokens_qmlplugin
    qindaqt_controls_qmlplugin
    qindaqt_settings_display_qmlplugin
    qindaqt_settings_appearance_qmlplugin
)
add_test(NAME qindaqt.display-page
         COMMAND qindaqt_display_page_tests)
set_tests_properties(
    qindaqt.display-page PROPERTIES
    LABELS "offscreen;accessibility"
    ENVIRONMENT
        "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_LINUX_ACCESSIBILITY_ALWAYS_ON=1"
)

qt_add_executable(qindaqt_display_night_light_section_tests
                  tst_display_night_light_section.cpp
                  stub_night_light_model.h)
target_include_directories(qindaqt_display_night_light_section_tests
                           PRIVATE ${PROJECT_SOURCE_DIR})
target_compile_definitions(
    qindaqt_display_night_light_section_tests PRIVATE
    QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
    QINDAQT_QML_IMPORT_PATH="${CMAKE_BINARY_DIR}/qml"
    QINDAQT_NIGHT_LIGHT_SECTION_QML_PATH="${PROJECT_SOURCE_DIR}/src/apps/settings/display/qml/DisplayNightLightSection.qml"
)
# The section imports QindaQt.Shell.Icons; a static module resolves only when
# its plugin is linked into the test binary (settings_center precedent).
target_link_libraries(
    qindaqt_display_night_light_section_tests
    PRIVATE qindaqt_settings_display qindaqt_settings_appearance QindaQt::TokensQml
            QindaQt::ShellIcons qindaqt_shell_iconsplugin
            Qt6::Quick Qt6::QuickControls2 Qt6::Test
)
qindaqt_enable_warnings(qindaqt_display_night_light_section_tests)
add_test(NAME qindaqt.display-night-light-section
         COMMAND qindaqt_display_night_light_section_tests)
set_tests_properties(qindaqt.display-night-light-section
                     PROPERTIES LABELS "app;display;night-light;qml"
                     ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_LINUX_ACCESSIBILITY_ALWAYS_ON=1")

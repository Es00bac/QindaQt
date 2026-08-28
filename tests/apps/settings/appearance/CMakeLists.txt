# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_executable(qindaqt_appearance_values_tests tst_appearance_values.cpp)
target_link_libraries(
    qindaqt_appearance_values_tests
    PRIVATE qindaqt_settings_appearance Qt6::Test
)
qindaqt_enable_warnings(qindaqt_appearance_values_tests)
add_test(NAME qindaqt.appearance-values COMMAND qindaqt_appearance_values_tests)

qt_add_executable(qindaqt_appearance_preview_tests tst_appearance_preview.cpp)
target_compile_definitions(
    qindaqt_appearance_preview_tests PRIVATE
    QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)
target_link_libraries(
    qindaqt_appearance_preview_tests
    PRIVATE qindaqt_settings_appearance Qt6::Test
)
qindaqt_enable_warnings(qindaqt_appearance_preview_tests)
add_test(NAME qindaqt.appearance-preview
         COMMAND qindaqt_appearance_preview_tests)

qt_add_executable(qindaqt_appearance_model_tests
                  tst_appearance_settings_model.cpp)
target_compile_definitions(
    qindaqt_appearance_model_tests PRIVATE
    QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)
target_link_libraries(
    qindaqt_appearance_model_tests
    PRIVATE qindaqt_settings_appearance Qt6::Test
)
qindaqt_enable_warnings(qindaqt_appearance_model_tests)
add_test(NAME qindaqt.appearance-settings-model
         COMMAND qindaqt_appearance_model_tests)

qt_add_executable(qindaqt_appearance_page_tests tst_appearance_page.cpp)
target_compile_definitions(
    qindaqt_appearance_page_tests PRIVATE
    QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
    QINDAQT_QML_IMPORT_PATH="${CMAKE_BINARY_DIR}/qml"
    QINDAQT_APPEARANCE_TEST_QML_DIR="${CMAKE_CURRENT_SOURCE_DIR}/qml"
)
target_link_libraries(
    qindaqt_appearance_page_tests
    PRIVATE qindaqt_settings_appearance QindaQt::TokensQml Qt6::Quick
            Qt6::QuickControls2 Qt6::Test
)
qindaqt_enable_warnings(qindaqt_appearance_page_tests)
# AGENT-CONTRACT: The page imports QindaQt.Tokens/Controls/SettingsApp.
# Appearance through the generated build-tree QML directories, so the module
# plugins must exist before the offscreen scene loads.
add_dependencies(
    qindaqt_appearance_page_tests
    qindaqt_tokens_qmlplugin
    qindaqt_controls_qmlplugin
    qindaqt_settings_appearance_qmlplugin
)
add_test(NAME qindaqt.appearance-page
         COMMAND qindaqt_appearance_page_tests)
set_tests_properties(
    qindaqt.appearance-page PROPERTIES
    LABELS "offscreen;accessibility"
    ENVIRONMENT
        "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_LINUX_ACCESSIBILITY_ALWAYS_ON=1"
)

# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_executable(qindaqt_design_token_derivation_tests tst_derivation.cpp)
target_link_libraries(
    qindaqt_design_token_derivation_tests
    PRIVATE QindaQt::DesignTokens qindaqt_themes Qt6::Test
)
target_compile_definitions(
    qindaqt_design_token_derivation_tests
    PRIVATE QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)
qindaqt_enable_warnings(qindaqt_design_token_derivation_tests)
add_test(NAME qindaqt.design-tokens-derivation COMMAND qindaqt_design_token_derivation_tests)

qt_add_executable(qindaqt_design_token_contrast_tests tst_builtin_contrast.cpp)
target_link_libraries(
    qindaqt_design_token_contrast_tests
    PRIVATE QindaQt::DesignTokens qindaqt_themes Qt6::Test
)
target_compile_definitions(
    qindaqt_design_token_contrast_tests
    PRIVATE QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)
qindaqt_enable_warnings(qindaqt_design_token_contrast_tests)
add_test(NAME qindaqt.design-tokens-built-in-contrast COMMAND qindaqt_design_token_contrast_tests)

qt_add_executable(qindaqt_design_token_qml_tests tst_token_facade.cpp)
target_link_libraries(
    qindaqt_design_token_qml_tests
    PRIVATE QindaQt::TokensQml qindaqt_themes Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Test
)
target_compile_definitions(
    qindaqt_design_token_qml_tests
    PRIVATE
        QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
        QINDAQT_QML_IMPORT_PATH="${CMAKE_BINARY_DIR}/qml"
)
add_dependencies(qindaqt_design_token_qml_tests qindaqt_tokens_qmlplugin)
qindaqt_enable_warnings(qindaqt_design_token_qml_tests)
add_test(NAME qindaqt.design-tokens-qml-singleton COMMAND qindaqt_design_token_qml_tests)
set_tests_properties(
    qindaqt.design-tokens-qml-singleton
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software"
)

qt_add_executable(qindaqt_design_token_benchmark tst_derivation_benchmark.cpp)
target_link_libraries(
    qindaqt_design_token_benchmark
    PRIVATE QindaQt::DesignTokens qindaqt_themes Qt6::Test
)
target_compile_definitions(
    qindaqt_design_token_benchmark
    PRIVATE QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)
qindaqt_enable_warnings(qindaqt_design_token_benchmark)
add_test(NAME qindaqt.design-tokens-benchmark COMMAND qindaqt_design_token_benchmark)
set_tests_properties(qindaqt.design-tokens-benchmark PROPERTIES LABELS benchmark)

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

# AGENT-CONTRACT: This must exercise a clean staged install, not build-tree
# include paths or targets. Keeping it in CTest prevents broad green suites
# from hiding a broken public C++ payload or an inaccurate acceptance fixture.
add_test(
    NAME qindaqt.design-tokens-installed-cpp-consumer
    COMMAND
        "${CMAKE_COMMAND}"
        "-DQINDAQT_CMAKE=${CMAKE_COMMAND}"
        "-DQINDAQT_BUILD_DIRECTORY=${CMAKE_BINARY_DIR}"
        "-DQINDAQT_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/installed-cpp-stage"
        "-DQINDAQT_CONSUMER_SOURCE_DIRECTORY=${CMAKE_CURRENT_SOURCE_DIR}/installed_consumer"
        "-DQINDAQT_CONSUMER_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/installed_cpp_consumer.cpp"
        "-DQINDAQT_INSTALL_LIBDIR=${CMAKE_INSTALL_LIBDIR}"
        "-DQINDAQT_INSTALL_INCLUDEDIR=${CMAKE_INSTALL_INCLUDEDIR}"
        "-DQINDAQT_INSTALL_DATADIR=${CMAKE_INSTALL_DATADIR}"
        "-DQINDAQT_DESIGN_TOKENS_LIBRARY_NAME=$<TARGET_FILE_NAME:qindaqt_design_tokens>"
        "-DQINDAQT_THEMES_LIBRARY_NAME=$<TARGET_FILE_NAME:qindaqt_themes>"
        "-DQINDAQT_QT6_DIR=${Qt6_DIR}"
        "-DQINDAQT_GENERATOR=${CMAKE_GENERATOR}"
        "-DQINDAQT_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
        "-DQINDAQT_CONFIGURATION=$<CONFIG>"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_installed_cpp_consumer.cmake"
)
set_tests_properties(
    qindaqt.design-tokens-installed-cpp-consumer PROPERTIES LABELS "package;design-tokens"
)

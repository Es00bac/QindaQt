# SPDX-License-Identifier: LGPL-3.0-or-later

qt_add_library(qindaqt_design_tokens STATIC)
add_library(QindaQt::DesignTokens ALIAS qindaqt_design_tokens)

target_sources(
    qindaqt_design_tokens
    PRIVATE
        src/accessibility_inputs.cpp
        src/color_math.cpp
        src/color_math_p.h
        src/design_tokens.cpp
        src/token_deriver.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/design_tokens/accessibility_inputs.h
            include/qindaqt/design_tokens/design_tokens.h
            include/qindaqt/design_tokens/token_deriver.h
)
target_link_libraries(qindaqt_design_tokens PUBLIC qindaqt_themes Qt6::Core Qt6::Gui)
target_compile_features(qindaqt_design_tokens PUBLIC cxx_std_20)
set_target_properties(
    qindaqt_design_tokens
    PROPERTIES CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON
)
qindaqt_enable_warnings(qindaqt_design_tokens)

set(QINDAQT_TOKENS_QML_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/qml/QindaQt/Tokens")
qt_add_library(qindaqt_tokens_qml SHARED)
add_library(QindaQt::TokensQml ALIAS qindaqt_tokens_qml)
qt_add_qml_module(
    qindaqt_tokens_qml
    URI QindaQt.Tokens
    VERSION 1.0
    CLASS_NAME QindaQtTokensPlugin
    OUTPUT_DIRECTORY "${QINDAQT_TOKENS_QML_OUTPUT_DIRECTORY}"
    TYPEINFO qindaqt_tokens.qmltypes
)
target_sources(
    qindaqt_tokens_qml
    PRIVATE src/token_facade.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES include/qindaqt/design_tokens/token_facade.h
)
target_include_directories(
    qindaqt_tokens_qml
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include/qindaqt/design_tokens"
)
target_link_libraries(
    qindaqt_tokens_qml
    PUBLIC QindaQt::DesignTokens Qt6::Core Qt6::Qml
)
target_compile_features(qindaqt_tokens_qml PUBLIC cxx_std_20)
set_target_properties(qindaqt_tokens_qml PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_tokens_qml)

install(
    TARGETS qindaqt_design_tokens
    EXPORT QindaQtTargets
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

# AGENT-NOTE: A QML module's backing library and plugin live beside qmldir.
# Installing either as a general-purpose library makes staged imports succeed
# accidentally in-tree but fail for downstream first-party applications.
install(
    TARGETS qindaqt_tokens_qml qindaqt_tokens_qmlplugin
    EXPORT QindaQtTargets
    RUNTIME DESTINATION "${QT6_INSTALL_QML}/QindaQt/Tokens"
        COMPONENT SettingsAppearanceRuntime
    LIBRARY DESTINATION "${QT6_INSTALL_QML}/QindaQt/Tokens"
        COMPONENT SettingsAppearanceRuntime
    ARCHIVE DESTINATION "${QT6_INSTALL_QML}/QindaQt/Tokens"
        COMPONENT SettingsAppearanceRuntime
    FILE_SET HEADERS DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
install(
    FILES
        "${QINDAQT_TOKENS_QML_OUTPUT_DIRECTORY}/qmldir"
        "${QINDAQT_TOKENS_QML_OUTPUT_DIRECTORY}/qindaqt_tokens.qmltypes"
    DESTINATION "${QT6_INSTALL_QML}/QindaQt/Tokens"
    COMPONENT SettingsAppearanceRuntime
)

# AGENT-CONTRACT: qindaqt_app_shell privately links QindaQt::TokensQml
# (src/app_shell/CMakeLists.txt), and the Text Editor links qindaqt_app_shell
# directly, so its narrow single-component staged install
# (tests/apps/text_editor/run_installed_editor.cmake, --component TextEditor)
# needs this shared library even though it does not use Settings Appearance's
# own QML plugin. Additive per-consumer COMPONENT, added here because
# src/design_tokens is not the Text Editor's owned path.
install(
    TARGETS qindaqt_tokens_qml
    LIBRARY DESTINATION "${QT6_INSTALL_QML}/QindaQt/Tokens"
    COMPONENT TextEditor
)

# SPDX-License-Identifier: LGPL-3.0-or-later

set(QINDAQT_CONTROLS_QML_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/qml/QindaQt/Controls")

qt_add_library(qindaqt_controls_qml SHARED)
add_library(QindaQt::ControlsQml ALIAS qindaqt_controls_qml)

qt_add_qml_module(
    qindaqt_controls_qml
    URI QindaQt.Controls
    VERSION 1.0
    OUTPUT_DIRECTORY "${QINDAQT_CONTROLS_QML_OUTPUT_DIRECTORY}"
    TYPEINFO qindaqt_controls.qmltypes
    QML_FILES
        qml/Button.qml
        qml/CheckBox.qml
        qml/DegradedNotice.qml
        qml/FocusRing.qml
        qml/FormRow.qml
        qml/FormSurface.qml
        qml/Label.qml
        qml/SectionHeader.qml
        qml/Slider.qml
        qml/StateCard.qml
        qml/Switch.qml
        qml/TextField.qml
        qml/ThemeCard.qml
        qml/TokenSwatch.qml
)

# Qt owns the deploy-name calculation used by qmldir and QML tooling. Query it
# once from the module target so installation cannot drift into a hand-maintained
# second public-type inventory.
qt_query_qml_module(
    qindaqt_controls_qml
    QML_FILES qindaqt_controls_qml_files
    QML_FILES_DEPLOY_PATHS qindaqt_controls_qml_deploy_paths
)
list(LENGTH qindaqt_controls_qml_files qindaqt_controls_qml_file_count)
list(LENGTH qindaqt_controls_qml_deploy_paths qindaqt_controls_deploy_path_count)
if(NOT qindaqt_controls_qml_file_count EQUAL qindaqt_controls_deploy_path_count)
    message(FATAL_ERROR "Controls QML sources and Qt deploy paths must remain one-to-one")
endif()
set_property(
    TARGET qindaqt_controls_qml
    PROPERTY QINDAQT_QML_DEPLOY_PATHS "${qindaqt_controls_qml_deploy_paths}"
)

foreach(qml_file deploy_path IN ZIP_LISTS
        qindaqt_controls_qml_files qindaqt_controls_qml_deploy_paths)
    cmake_path(GET deploy_path PARENT_PATH deploy_directory)
    cmake_path(GET deploy_path FILENAME deploy_name)
    # AGENT-CONTRACT: Installed tooling consumes the same relative paths Qt
    # generated into qmldir. Preserve aliases/subdirectories from Qt's query;
    # installing only compiled resources is insufficient for qmllint/qmlls.
    install(
        FILES "${qml_file}"
        DESTINATION "${QT6_INSTALL_QML}/QindaQt/Controls/${deploy_directory}"
        RENAME "${deploy_name}"
        COMPONENT SettingsAppearanceRuntime
    )
endforeach()

target_link_libraries(
    qindaqt_controls_qml
    PRIVATE
        QindaQt::TokensQml
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
)
set_target_properties(
    qindaqt_controls_qml
    PROPERTIES
        CXX_EXTENSIONS OFF
        # AGENT-CONTRACT: Controls and Tokens install as sibling QML modules.
        # Keep this relative so a staged or relocated prefix remains runnable.
        INSTALL_RPATH "$ORIGIN/../Tokens"
)
qindaqt_enable_warnings(qindaqt_controls_qml)
add_dependencies(qindaqt_controls_qml qindaqt_tokens_qmlplugin)

# AGENT-NOTE: The backing library contains the compiled QML resources. Both it
# and the generated plugin must remain beside qmldir in an installed prefix;
# moving one to the general library directory breaks clean first-party imports.
install(
    TARGETS qindaqt_controls_qml qindaqt_controls_qmlplugin
    EXPORT QindaQtTargets
    RUNTIME DESTINATION "${QT6_INSTALL_QML}/QindaQt/Controls"
        COMPONENT SettingsAppearanceRuntime
    LIBRARY DESTINATION "${QT6_INSTALL_QML}/QindaQt/Controls"
        COMPONENT SettingsAppearanceRuntime
    ARCHIVE DESTINATION "${QT6_INSTALL_QML}/QindaQt/Controls"
        COMPONENT SettingsAppearanceRuntime
)
install(
    FILES
        "${QINDAQT_CONTROLS_QML_OUTPUT_DIRECTORY}/qmldir"
        "${QINDAQT_CONTROLS_QML_OUTPUT_DIRECTORY}/qindaqt_controls.qmltypes"
    DESTINATION "${QT6_INSTALL_QML}/QindaQt/Controls"
    COMPONENT SettingsAppearanceRuntime
)

# AGENT-CONTRACT: qindaqt_app_shell privately links QindaQt::ControlsQml
# (src/app_shell/CMakeLists.txt), and the Text Editor links qindaqt_app_shell
# directly, so its narrow single-component staged install
# (tests/apps/text_editor/run_installed_editor.cmake, --component TextEditor)
# needs this shared library even though it does not use Settings Appearance's
# own QML plugin. Additive per-consumer COMPONENT, added here because
# src/controls is not the Text Editor's owned path.
install(
    TARGETS qindaqt_controls_qml
    LIBRARY DESTINATION "${QT6_INSTALL_QML}/QindaQt/Controls"
    COMPONENT TextEditor
)

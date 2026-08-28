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
    LIBRARY DESTINATION "${QT6_INSTALL_QML}/QindaQt/Controls"
    ARCHIVE DESTINATION "${QT6_INSTALL_QML}/QindaQt/Controls"
)
install(
    FILES
        "${QINDAQT_CONTROLS_QML_OUTPUT_DIRECTORY}/qmldir"
        "${QINDAQT_CONTROLS_QML_OUTPUT_DIRECTORY}/qindaqt_controls.qmltypes"
    DESTINATION "${QT6_INSTALL_QML}/QindaQt/Controls"
)

# SPDX-License-Identifier: GPL-3.0-or-later

# Display domain of the first-party settings application: validated
# display topology values, resolution/scale/orientation drafts, and the
# Display1-backed reversible transaction coordinator model.
qt_add_library(qindaqt_settings_display STATIC)
target_sources(
    qindaqt_settings_display
    PRIVATE
        display_settings_values.cpp
        display_settings_draft.cpp
        display_settings_model.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/apps/settings_display/display_settings_values.h
            include/qindaqt/apps/settings_display/display_settings_model.h
)
target_link_libraries(
    qindaqt_settings_display
    PUBLIC QindaQt::DisplayClient QindaQt::DisplayTopology QindaQt::DisplayProtocol
           Qt6::Core Qt6::Gui Qt6::Qml
)
target_compile_features(qindaqt_settings_display PUBLIC cxx_std_20)
set_target_properties(
    qindaqt_settings_display
    PROPERTIES CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON
)
qindaqt_enable_warnings(qindaqt_settings_display)

set(QINDAQT_DISPLAY_QML_OUTPUT_DIRECTORY
    "${CMAKE_BINARY_DIR}/qml/QindaQt/SettingsApp/Display")
qt_add_library(qindaqt_settings_display_qml SHARED)
qt_add_qml_module(
    qindaqt_settings_display_qml
    URI QindaQt.SettingsApp.Display
    VERSION 1.0
    CLASS_NAME QindaQtSettingsDisplayPlugin
    OUTPUT_DIRECTORY "${QINDAQT_DISPLAY_QML_OUTPUT_DIRECTORY}"
    TYPEINFO qindaqt_settings_display.qmltypes
    QML_FILES
        qml/DisplayArrangementSection.qml
        qml/DisplayModeSection.qml
        qml/DisplayOutputCard.qml
        qml/DisplayOutputSection.qml
        qml/DisplayPage.qml
        qml/DisplayPreviewBanner.qml
        qml/DisplayScaleSection.qml
        qml/DisplayTransformSection.qml
)
target_link_libraries(
    qindaqt_settings_display_qml
    PRIVATE QindaQt::TokensQml Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick
            Qt6::QuickControls2
)
set_target_properties(
    qindaqt_settings_display_qml
    PROPERTIES
        CXX_EXTENSIONS OFF
        INSTALL_RPATH "$ORIGIN/../../Tokens;$ORIGIN/../../Controls"
)
qindaqt_enable_warnings(qindaqt_settings_display_qml)

install(
    TARGETS qindaqt_settings_display_qml qindaqt_settings_display_qmlplugin
    RUNTIME DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Display"
        COMPONENT SettingsAppearanceRuntime
    LIBRARY DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Display"
        COMPONENT SettingsAppearanceRuntime
    ARCHIVE DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Display"
        COMPONENT SettingsAppearanceRuntime
)
install(
    FILES
        "${QINDAQT_DISPLAY_QML_OUTPUT_DIRECTORY}/qmldir"
        "${QINDAQT_DISPLAY_QML_OUTPUT_DIRECTORY}/qindaqt_settings_display.qmltypes"
    DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Display"
    COMPONENT SettingsAppearanceRuntime
)
install(
    FILES
        qml/DisplayArrangementSection.qml
        qml/DisplayModeSection.qml
        qml/DisplayOutputCard.qml
        qml/DisplayOutputSection.qml
        qml/DisplayPage.qml
        qml/DisplayPreviewBanner.qml
        qml/DisplayScaleSection.qml
        qml/DisplayTransformSection.qml
    DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Display"
    COMPONENT SettingsAppearanceRuntime
)

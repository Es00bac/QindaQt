# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_library(qindaqt_settings_accessibility STATIC)
target_sources(
    qindaqt_settings_accessibility
    PRIVATE
        accessibility_settings_model.cpp
        accessibility_values.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/apps/settings_accessibility/accessibility_settings_model.h
            include/qindaqt/apps/settings_accessibility/accessibility_values.h
)
target_link_libraries(
    qindaqt_settings_accessibility
    PUBLIC QindaQt::SettingsClient Qt6::Core
)
target_compile_features(qindaqt_settings_accessibility PUBLIC cxx_std_20)
set_target_properties(
    qindaqt_settings_accessibility
    PROPERTIES CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON
)
qindaqt_enable_warnings(qindaqt_settings_accessibility)

set(QINDAQT_ACCESSIBILITY_SETTINGS_QML_OUTPUT_DIRECTORY
    "${CMAKE_BINARY_DIR}/qml/QindaQt/SettingsApp/Accessibility")
qt_add_library(qindaqt_settings_accessibility_qml SHARED)
qt_add_qml_module(
    qindaqt_settings_accessibility_qml
    URI QindaQt.SettingsApp.Accessibility
    VERSION 1.0
    CLASS_NAME QindaQtSettingsAccessibilityPlugin
    OUTPUT_DIRECTORY "${QINDAQT_ACCESSIBILITY_SETTINGS_QML_OUTPUT_DIRECTORY}"
    TYPEINFO qindaqt_settings_accessibility.qmltypes
    QML_FILES qml/AccessibilityPage.qml
)
target_link_libraries(
    qindaqt_settings_accessibility_qml
    PRIVATE QindaQt::TokensQml
            Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2
)
set_target_properties(
    qindaqt_settings_accessibility_qml PROPERTIES
    CXX_EXTENSIONS OFF
    INSTALL_RPATH "$ORIGIN/../../Tokens;$ORIGIN/../../Controls"
)
qindaqt_enable_warnings(qindaqt_settings_accessibility_qml)

# AGENT-NOTE: The route ships inside the shared SettingsAppearanceRuntime
# component like every other Settings route. The staged installed-route gates
# and the release packaging install exactly that one component, so a separate
# component would leave the Accessibility module out of the relocated product.
install(
    TARGETS qindaqt_settings_accessibility_qml qindaqt_settings_accessibility_qmlplugin
    RUNTIME DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Accessibility"
        COMPONENT SettingsAppearanceRuntime
    LIBRARY DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Accessibility"
        COMPONENT SettingsAppearanceRuntime
    ARCHIVE DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Accessibility"
        COMPONENT SettingsAppearanceRuntime
)
install(
    FILES
        "${QINDAQT_ACCESSIBILITY_SETTINGS_QML_OUTPUT_DIRECTORY}/qmldir"
        "${QINDAQT_ACCESSIBILITY_SETTINGS_QML_OUTPUT_DIRECTORY}/qindaqt_settings_accessibility.qmltypes"
        qml/AccessibilityPage.qml
    DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Accessibility"
    COMPONENT SettingsAppearanceRuntime
)

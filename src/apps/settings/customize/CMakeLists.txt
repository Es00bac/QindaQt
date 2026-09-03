# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_library(qindaqt_settings_customize STATIC)
target_sources(
    qindaqt_settings_customize
    PRIVATE
        customize_catalog.cpp
        customize_editor_host.cpp
        customize_settings_actions.cpp
        customize_settings_model.cpp
        customize_settings_projection.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/apps/settings_customize/customize_editor_host.h
            include/qindaqt/apps/settings_customize/customize_settings_model.h
)
target_link_libraries(
    qindaqt_settings_customize
    PUBLIC
        QindaQt::SettingsClient
        QindaQt::ShellCustomizationEditor
        Qt6::Core Qt6::Qml
)
target_compile_features(qindaqt_settings_customize PUBLIC cxx_std_20)
set_target_properties(
    qindaqt_settings_customize PROPERTIES
    CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON
)
qindaqt_enable_warnings(qindaqt_settings_customize)

set(QINDAQT_CUSTOMIZE_QML_OUTPUT_DIRECTORY
    "${CMAKE_BINARY_DIR}/qml/QindaQt/SettingsApp/Customize")
qt_add_library(qindaqt_settings_customize_qml STATIC)
qt_add_qml_module(
    qindaqt_settings_customize_qml
    URI QindaQt.SettingsApp.Customize
    VERSION 1.0
    CLASS_NAME QindaQtSettingsCustomizePlugin
    OUTPUT_DIRECTORY "${QINDAQT_CUSTOMIZE_QML_OUTPUT_DIRECTORY}"
    TYPEINFO qindaqt_settings_customize.qmltypes
    SOURCES
        customize_route_composition.h
        customize_route_composition.cpp
    QML_FILES
        qml/CustomizeAppletPalette.qml
        qml/CustomizeAppletProperties.qml
        qml/CustomizeActionBar.qml
        qml/CustomizeCanvas.qml
        qml/CustomizeOutline.qml
        qml/CustomizePage.qml
        qml/CustomizePanelPositionProperties.qml
        qml/CustomizePanelSizeProperties.qml
        qml/CustomizePanelVisibilityProperties.qml
        qml/CustomizeProperties.qml
        qml/CustomizeRoute.qml
)
target_link_libraries(
    qindaqt_settings_customize_qml
    PRIVATE
        qindaqt_settings_customize QindaQt::TokensQml
        Qt6::Core Qt6::DBus Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2
)
target_compile_definitions(
    qindaqt_settings_customize_qml PRIVATE
    QINDAQT_CUSTOMIZE_BUILD_EXECUTABLE_PATH="${CMAKE_BINARY_DIR}/src/apps/settings_center/qindaqt-settings${CMAKE_EXECUTABLE_SUFFIX}"
    QINDAQT_CUSTOMIZE_INSTALL_DATA_RELATIVE_PATH="../${CMAKE_INSTALL_DATADIR}/qindaqt"
    QINDAQT_CUSTOMIZE_SOURCE_PROFILE_DIRECTORY="${PROJECT_SOURCE_DIR}/data/profiles"
    QINDAQT_CUSTOMIZE_SOURCE_MANIFEST_DIRECTORY="${PROJECT_SOURCE_DIR}/data/applets"
)
set_target_properties(
    qindaqt_settings_customize_qml PROPERTIES
    CXX_EXTENSIONS OFF
    INSTALL_RPATH "$ORIGIN/../../Tokens;$ORIGIN/../../Controls"
)
qindaqt_enable_warnings(qindaqt_settings_customize_qml)

install(
    TARGETS qindaqt_settings_customize_qml qindaqt_settings_customize_qmlplugin
    RUNTIME DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Customize"
        COMPONENT SettingsAppearanceRuntime
    LIBRARY DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Customize"
        COMPONENT SettingsAppearanceRuntime
    ARCHIVE DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Customize"
        COMPONENT SettingsAppearanceRuntime
)
install(
    FILES
        "${QINDAQT_CUSTOMIZE_QML_OUTPUT_DIRECTORY}/qmldir"
        "${QINDAQT_CUSTOMIZE_QML_OUTPUT_DIRECTORY}/qindaqt_settings_customize.qmltypes"
    DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Customize"
    COMPONENT SettingsAppearanceRuntime
)
install(
    FILES
        qml/CustomizeAppletPalette.qml
        qml/CustomizeAppletProperties.qml
        qml/CustomizeActionBar.qml
        qml/CustomizeCanvas.qml
        qml/CustomizeOutline.qml
        qml/CustomizePage.qml
        qml/CustomizePanelPositionProperties.qml
        qml/CustomizePanelSizeProperties.qml
        qml/CustomizePanelVisibilityProperties.qml
        qml/CustomizeProperties.qml
        qml/CustomizeRoute.qml
    DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Customize"
    COMPONENT SettingsAppearanceRuntime
)
install(
    DIRECTORY "${PROJECT_SOURCE_DIR}/data/profiles/"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/qindaqt/profiles"
    COMPONENT SettingsAppearanceRuntime
)
install(
    DIRECTORY "${PROJECT_SOURCE_DIR}/data/applets/"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/qindaqt/applets"
    COMPONENT SettingsAppearanceRuntime
)

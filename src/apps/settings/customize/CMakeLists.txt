# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_library(qindaqt_settings_customize STATIC)
target_sources(
    qindaqt_settings_customize
    PRIVATE
        customize_editor_host.cpp
        customize_preset_catalog.cpp
        customize_settings_model.cpp
        customize_settings_panel_delay.cpp
        customize_settings_presets.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/apps/settings_customize/customize_editor_host.h
            include/qindaqt/apps/settings_customize/customize_preset_catalog.h
            include/qindaqt/apps/settings_customize/customize_settings_model.h
)
target_link_libraries(
    qindaqt_settings_customize
    PUBLIC
        QindaQt::SettingsClient
        # AGENT-NOTE: only customize_editor_host needs the editor domain now;
        # it stays as the reference composition for the live-host parity rows
        # (ADR-0267). The preset model itself needs profiles and Settings1.
        QindaQt::ShellCustomizationEditor
        qindaqt_profiles
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
    DEPENDENCIES
        TARGET qindaqt_shell_icons
    SOURCES
        customize_route_composition.h
        customize_route_composition.cpp
    QML_FILES
        qml/CustomizeIconButton.qml
        qml/CustomizePage.qml
        qml/CustomizePanelOptions.qml
        qml/CustomizePresetNameDialog.qml
        qml/CustomizePresetSection.qml
        qml/CustomizeProfileCard.qml
        qml/CustomizeRoute.qml
)
target_link_libraries(
    qindaqt_settings_customize_qml
    PRIVATE
        qindaqt_settings_customize
        QindaQt::TokensQml QindaQt::ShellIcons
        Qt6::Core Qt6::DBus Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2
)
add_dependencies(
    qindaqt_settings_customize_qml
    qindaqt_shell_iconsplugin
)
target_compile_definitions(
    qindaqt_settings_customize_qml PRIVATE
    QINDAQT_CUSTOMIZE_BUILD_EXECUTABLE_PATH="${CMAKE_BINARY_DIR}/src/apps/settings_center/qindaqt-settings${CMAKE_EXECUTABLE_SUFFIX}"
    QINDAQT_CUSTOMIZE_INSTALL_DATA_RELATIVE_PATH="../${CMAKE_INSTALL_DATADIR}/qindaqt"
    QINDAQT_CUSTOMIZE_SOURCE_PROFILE_DIRECTORY="${PROJECT_SOURCE_DIR}/data/profiles"
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
        qml/CustomizeIconButton.qml
        qml/CustomizePage.qml
        qml/CustomizePanelOptions.qml
        qml/CustomizePresetNameDialog.qml
        qml/CustomizePresetSection.qml
        qml/CustomizeProfileCard.qml
        qml/CustomizeRoute.qml
    DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Customize"
    COMPONENT SettingsAppearanceRuntime
)
# Stage the confined icon module beside the route so an installed Settings
# package can resolve QindaQt.Shell.Icons without a shell install.
include("${PROJECT_SOURCE_DIR}/src/shell/IconRuntimeInstall.cmake")
qindaqt_install_shell_icons_runtime(SettingsAppearanceRuntime)
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

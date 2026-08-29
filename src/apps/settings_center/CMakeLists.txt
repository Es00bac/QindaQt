# SPDX-License-Identifier: GPL-3.0-or-later
qt_add_library(
    qindaqt_settings_navigation STATIC
    settings_route.h
    settings_route.cpp
    settings_route_registry.h
    settings_route_registry.cpp
    settings_navigation_controller.h
    settings_navigation_controller.cpp
)
add_library(QindaQt::SettingsNavigation ALIAS qindaqt_settings_navigation)
target_include_directories(
    qindaqt_settings_navigation PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}"
)
target_link_libraries(qindaqt_settings_navigation PUBLIC Qt6::Core)
target_compile_features(qindaqt_settings_navigation PUBLIC cxx_std_20)
set_target_properties(
    qindaqt_settings_navigation PROPERTIES
    CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON
)
qindaqt_enable_warnings(qindaqt_settings_navigation)

qt_add_executable(qindaqt-settings main.cpp)
qt_add_qml_module(
    qindaqt-settings
    URI QindaQt.SettingsApp
    VERSION 1.0
    OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/qml/QindaQt/SettingsApp"
    QML_FILES
        Main.qml
        NotificationsPage.qml
        SettingsSidebar.qml
        SettingsCompactHeader.qml
        SettingsNavButton.qml
        SettingsRouteHost.qml
)
target_link_libraries(
    qindaqt-settings PRIVATE
    QindaQt::SettingsClient
    QindaQt::DisplayClient
    QindaQt::SettingsNavigation
    qindaqt_settings_appearance
    qindaqt_settings_display
    qindaqt_design_tokens
    qindaqt_tokens_qml
    qindaqt_controls_qml
    Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2
)
target_compile_definitions(
    qindaqt-settings PRIVATE
    QINDAQT_BUILD_EXECUTABLE_PATH="${CMAKE_CURRENT_BINARY_DIR}/qindaqt-settings${CMAKE_EXECUTABLE_SUFFIX}"
    QINDAQT_BUILD_QML_IMPORT_PATH="${CMAKE_BINARY_DIR}/qml"
    QINDAQT_INSTALL_QML_RELATIVE_PATH="../${QT6_INSTALL_QML}"
    QINDAQT_INSTALL_THEME_RELATIVE_PATH="../${CMAKE_INSTALL_DATADIR}/qindaqt/themes"
)
set_target_properties(
    qindaqt-settings PROPERTIES
    INSTALL_RPATH "$ORIGIN/../${QT6_INSTALL_QML}/QindaQt/Tokens;$ORIGIN/../${QT6_INSTALL_QML}/QindaQt/Controls;$ORIGIN/../${QT6_INSTALL_QML}/QindaQt/SettingsApp/Display"
)
add_dependencies(
    qindaqt-settings
    qindaqt_settings_appearance_qml qindaqt_settings_appearance_qmlplugin
    qindaqt_settings_display_qml qindaqt_settings_display_qmlplugin
    qindaqt_controls_qml qindaqt_controls_qmlplugin
    qindaqt_tokens_qml qindaqt_tokens_qmlplugin
)
qindaqt_enable_warnings(qindaqt-settings)
install(
    TARGETS qindaqt-settings
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    COMPONENT SettingsAppearanceRuntime
)
install(
    FILES org.qindaqt.Settings.desktop
    DESTINATION "${CMAKE_INSTALL_DATADIR}/applications"
    COMPONENT SettingsAppearanceRuntime
)

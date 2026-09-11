# SPDX-License-Identifier: GPL-3.0-or-later

# The statically composed Color backend uses DisplayWriter's private Wayland
# implementation. Qt's deferred executable finalizer resolves those imported
# link targets in this directory scope.
find_package(Qt6 6.11 REQUIRED COMPONENTS WaylandClient)
find_package(Wayland REQUIRED COMPONENTS Client)

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
    QindaQt::FontDiscovery
    QindaQt::NetworkQtTransport
    QindaQt::AudioClient
    QindaQt::BluetoothClient
    QindaQt::SettingsNavigation
    QindaQt::ShellIcons
    qindaqt_settings_appearance
    qindaqt_settings_display
    qindaqt_settings_network
    qindaqt_settings_customize_qml
    qindaqt_settings_customize_qmlplugin
    qindaqt_settings_audio
    qindaqt_settings_bluetooth
    qindaqt_settings_power_backend_qml
    qindaqt_settings_power_backend_qmlplugin
    qindaqt_settings_clipboard_qml
    qindaqt_settings_clipboard_qmlplugin
    qindaqt_settings_color_backend_qml
    qindaqt_settings_color_backend_qmlplugin
    qindaqt_settings_accessibility
    qindaqt_settings_input_backend_qml
    qindaqt_settings_input_backend_qmlplugin
    qindaqt_design_tokens
    qindaqt_tokens_qml
    qindaqt_controls_qml
    Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2 Qt6::Widgets
)
target_compile_definitions(
    qindaqt-settings PRIVATE
    QINDAQT_BUILD_EXECUTABLE_PATH="${CMAKE_CURRENT_BINARY_DIR}/qindaqt-settings${CMAKE_EXECUTABLE_SUFFIX}"
    QINDAQT_BUILD_QML_IMPORT_PATH="${CMAKE_BINARY_DIR}/qml"
    QINDAQT_INSTALL_QML_RELATIVE_PATH="../${QT6_INSTALL_QML}"
    QINDAQT_INSTALL_THEME_RELATIVE_PATH="../${CMAKE_INSTALL_DATADIR}/qindaqt/themes"
    QINDAQT_INSTALL_WALLPAPER_RELATIVE_PATH="../${CMAKE_INSTALL_DATADIR}/qindaqt/wallpapers"
    QINDAQT_INSTALL_ICON_RELATIVE_PATH="../${CMAKE_INSTALL_DATADIR}/icons"
    QINDAQT_SOURCE_ICON_DIRECTORY="${PROJECT_SOURCE_DIR}/data/icons"
)
set_target_properties(
    qindaqt-settings PROPERTIES
    INSTALL_RPATH "$ORIGIN/../${QT6_INSTALL_QML}/QindaQt/Tokens;$ORIGIN/../${QT6_INSTALL_QML}/QindaQt/Controls;$ORIGIN/../${QT6_INSTALL_QML}/QindaQt/SettingsApp/Display;$ORIGIN/../${QT6_INSTALL_QML}/QindaQt/SettingsApp/Network;$ORIGIN/../${QT6_INSTALL_QML}/QindaQt/SettingsApp/Customize;$ORIGIN/../${QT6_INSTALL_QML}/QindaQt/SettingsApp/Audio;$ORIGIN/../${QT6_INSTALL_QML}/QindaQt/SettingsApp/Bluetooth;$ORIGIN/../${QT6_INSTALL_QML}/QindaQt/SettingsApp/Power;$ORIGIN/../${QT6_INSTALL_QML}/QindaQt/SettingsApp/Clipboard;$ORIGIN/../${QT6_INSTALL_QML}/QindaQt/SettingsApp/Color;$ORIGIN/../${QT6_INSTALL_QML}/QindaQt/SettingsApp/Accessibility;$ORIGIN/../${QT6_INSTALL_QML}/QindaQt/SettingsApp/Input"
)
add_dependencies(
    qindaqt-settings
    qindaqt_settings_appearance_qml qindaqt_settings_appearance_qmlplugin
    qindaqt_settings_display_qml qindaqt_settings_display_qmlplugin
    qindaqt_settings_network_qml qindaqt_settings_network_qmlplugin
    qindaqt_settings_customize_qml qindaqt_settings_customize_qmlplugin
    qindaqt_settings_audio_qml qindaqt_settings_audio_qmlplugin
    qindaqt_settings_bluetooth_qml qindaqt_settings_bluetooth_qmlplugin
    qindaqt_settings_power_backend_qml qindaqt_settings_power_backend_qmlplugin
    qindaqt_settings_power_qml qindaqt_settings_power_qmlplugin
    qindaqt_settings_clipboard_qml qindaqt_settings_clipboard_qmlplugin
    qindaqt_settings_color_backend_qml qindaqt_settings_color_backend_qmlplugin
    qindaqt_settings_color_qml qindaqt_settings_color_qmlplugin
    qindaqt_settings_accessibility_qml qindaqt_settings_accessibility_qmlplugin
    qindaqt_settings_input_qml qindaqt_settings_input_qmlplugin
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

# SPDX-License-Identifier: GPL-3.0-or-later
qt_add_executable(qindaqt-settings main.cpp)
qt_add_qml_module(
    qindaqt-settings
    URI QindaQt.SettingsApp
    VERSION 1.0
    QML_FILES Main.qml NotificationsPage.qml
)
target_link_libraries(
    qindaqt-settings PRIVATE QindaQt::SettingsClient
    qindaqt_settings_appearance
    Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2
)
# The route page resolves the appearance QML module through the engine import
# path at runtime (see main.cpp), so only build ordering is needed here.
add_dependencies(
    qindaqt-settings
    qindaqt_settings_appearance_qml qindaqt_settings_appearance_qmlplugin
)
qindaqt_enable_warnings(qindaqt-settings)
install(TARGETS qindaqt-settings RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(FILES org.qindaqt.Settings.desktop DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")

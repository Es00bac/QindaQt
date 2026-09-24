# SPDX-License-Identifier: GPL-3.0-or-later

# The Screen saver route (ADR-0226): the model layer over the purpose-scoped
# Settings1 pair `power.screensaver` / `power.screensaverMinutes`, the lock
# screen saver mirror (kscreenlockerrc [Greeter] only), and the unlocked
# saver/black-window preview boundary. The walk-away lock section on the same
# page reuses
# qindaqt_settings_screen_lock; neither store may write the other's group.
qt_add_library(qindaqt_settings_screensaver STATIC)
target_sources(
    qindaqt_settings_screensaver
    PRIVATE
        screensaver_settings_model.cpp
        lock_screen_saver_store.cpp
        screensaver_preview.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/apps/settings_screensaver/screensaver_settings_model.h
            include/qindaqt/apps/settings_screensaver/lock_screen_saver_store.h
            include/qindaqt/apps/settings_screensaver/screensaver_preview.h
)
# The greeter mirror writes KScreenLocker's own KConfig file, so it uses
# KConfig rather than QSettings: the wallpaper plugin's configuration lives in
# a nested `[Greeter][Wallpaper][...][General]` group that QSettings would
# escape into a different section name.
find_package(KF6Config 6.0 REQUIRED CONFIG)

target_link_libraries(
    qindaqt_settings_screensaver
    PUBLIC QindaQt::DesktopControls QindaQt::SettingsClient Qt6::Core
    PRIVATE KF6::ConfigCore Qt6::Quick
)
target_compile_features(qindaqt_settings_screensaver PUBLIC cxx_std_20)
set_target_properties(
    qindaqt_settings_screensaver
    PROPERTIES CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON
)
qindaqt_enable_warnings(qindaqt_settings_screensaver)

install(
    TARGETS qindaqt_settings_screensaver
    EXPORT QindaQtTargets
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

set(QINDAQT_SCREENSAVER_SETTINGS_QML_OUTPUT_DIRECTORY
    "${CMAKE_BINARY_DIR}/qml/QindaQt/SettingsApp/ScreenSaver")
qt_add_library(qindaqt_settings_screensaver_qml STATIC)
qt_add_qml_module(
    qindaqt_settings_screensaver_qml
    URI QindaQt.SettingsApp.ScreenSaver
    VERSION 1.0
    CLASS_NAME QindaQtSettingsScreenSaverPlugin
    OUTPUT_DIRECTORY "${QINDAQT_SCREENSAVER_SETTINGS_QML_OUTPUT_DIRECTORY}"
    TYPEINFO qindaqt_settings_screensaver.qmltypes
    SOURCES
        screensaver_route_composition.h
        screensaver_route_composition.cpp
    QML_FILES qml/ScreensaverPage.qml
)
target_link_libraries(
    qindaqt_settings_screensaver_qml
    PRIVATE
        qindaqt_settings_screensaver qindaqt_settings_screen_lock
        QindaQt::DesktopControls QindaQt::SettingsClient QindaQt::TokensQml
        Qt6::Core Qt6::DBus Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2
)
set_target_properties(
    qindaqt_settings_screensaver_qml PROPERTIES
    CXX_EXTENSIONS OFF
    INSTALL_RPATH "$ORIGIN/../../Tokens;$ORIGIN/../../Controls"
)
qindaqt_enable_warnings(qindaqt_settings_screensaver_qml)

install(
    TARGETS qindaqt_settings_screensaver_qml qindaqt_settings_screensaver_qmlplugin
    RUNTIME DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/ScreenSaver"
        COMPONENT SettingsAppearanceRuntime
    LIBRARY DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/ScreenSaver"
        COMPONENT SettingsAppearanceRuntime
    ARCHIVE DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/ScreenSaver"
        COMPONENT SettingsAppearanceRuntime
)
install(
    FILES
        "${QINDAQT_SCREENSAVER_SETTINGS_QML_OUTPUT_DIRECTORY}/qmldir"
        "${QINDAQT_SCREENSAVER_SETTINGS_QML_OUTPUT_DIRECTORY}/qindaqt_settings_screensaver.qmltypes"
        qml/ScreensaverPage.qml
    DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/ScreenSaver"
    COMPONENT SettingsAppearanceRuntime
)

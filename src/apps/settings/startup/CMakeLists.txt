# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_library(qindaqt_settings_startup STATIC)
target_sources(
    qindaqt_settings_startup
    PRIVATE
        startup_settings_model.cpp
        xdg_autostart_store.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/apps/settings_startup/startup_settings_model.h
)
target_link_libraries(qindaqt_settings_startup PUBLIC Qt6::Core)
target_compile_features(qindaqt_settings_startup PUBLIC cxx_std_20)
set_target_properties(
    qindaqt_settings_startup
    PROPERTIES CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON
)
qindaqt_enable_warnings(qindaqt_settings_startup)

install(
    TARGETS qindaqt_settings_startup
    EXPORT QindaQtTargets
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

set(QINDAQT_STARTUP_SETTINGS_QML_OUTPUT_DIRECTORY
    "${CMAKE_BINARY_DIR}/qml/QindaQt/SettingsApp/Startup")
qt_add_library(qindaqt_settings_startup_qml STATIC)
qt_add_qml_module(
    qindaqt_settings_startup_qml
    URI QindaQt.SettingsApp.Startup
    VERSION 1.0
    CLASS_NAME QindaQtSettingsStartupPlugin
    OUTPUT_DIRECTORY "${QINDAQT_STARTUP_SETTINGS_QML_OUTPUT_DIRECTORY}"
    TYPEINFO qindaqt_settings_startup.qmltypes
    SOURCES
        startup_route_composition.h
        startup_route_composition.cpp
    QML_FILES qml/StartupPage.qml
)
target_link_libraries(
    qindaqt_settings_startup_qml
    PRIVATE
        qindaqt_settings_startup QindaQt::TokensQml
        Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2
)
set_target_properties(
    qindaqt_settings_startup_qml PROPERTIES
    CXX_EXTENSIONS OFF
    INSTALL_RPATH "$ORIGIN/../../Tokens;$ORIGIN/../../Controls"
)
qindaqt_enable_warnings(qindaqt_settings_startup_qml)

install(
    TARGETS qindaqt_settings_startup_qml qindaqt_settings_startup_qmlplugin
    RUNTIME DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Startup"
        COMPONENT SettingsAppearanceRuntime
    LIBRARY DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Startup"
        COMPONENT SettingsAppearanceRuntime
    ARCHIVE DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Startup"
        COMPONENT SettingsAppearanceRuntime
)
install(
    FILES
        "${QINDAQT_STARTUP_SETTINGS_QML_OUTPUT_DIRECTORY}/qmldir"
        "${QINDAQT_STARTUP_SETTINGS_QML_OUTPUT_DIRECTORY}/qindaqt_settings_startup.qmltypes"
        qml/StartupPage.qml
    DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Startup"
    COMPONENT SettingsAppearanceRuntime
)

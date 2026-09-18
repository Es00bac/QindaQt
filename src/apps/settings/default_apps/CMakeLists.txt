# SPDX-License-Identifier: GPL-3.0-or-later

find_package(KF6Config 6.0 REQUIRED CONFIG)

qt_add_library(qindaqt_settings_default_apps STATIC)
target_sources(
    qindaqt_settings_default_apps
    PRIVATE
        default_applications_store.cpp
        default_applications_catalog.cpp
        default_applications_settings_model.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/apps/settings_default_apps/default_applications_store.h
            include/qindaqt/apps/settings_default_apps/default_applications_catalog.h
            include/qindaqt/apps/settings_default_apps/default_applications_settings_model.h
)
target_link_libraries(
    qindaqt_settings_default_apps
    PUBLIC QindaQt::ApplicationCatalog KF6::ConfigCore Qt6::Core
)
target_compile_features(qindaqt_settings_default_apps PUBLIC cxx_std_20)
set_target_properties(
    qindaqt_settings_default_apps
    PROPERTIES CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON
)
qindaqt_enable_warnings(qindaqt_settings_default_apps)

set(QINDAQT_DEFAULT_APPS_SETTINGS_QML_OUTPUT_DIRECTORY
    "${CMAKE_BINARY_DIR}/qml/QindaQt/SettingsApp/DefaultApplications")
qt_add_library(qindaqt_settings_default_apps_qml STATIC)
qt_add_qml_module(
    qindaqt_settings_default_apps_qml
    URI QindaQt.SettingsApp.DefaultApplications
    VERSION 1.0
    CLASS_NAME QindaQtSettingsDefaultApplicationsPlugin
    OUTPUT_DIRECTORY "${QINDAQT_DEFAULT_APPS_SETTINGS_QML_OUTPUT_DIRECTORY}"
    TYPEINFO qindaqt_settings_default_apps.qmltypes
    SOURCES
        default_applications_route_composition.h
        default_applications_route_composition.cpp
    QML_FILES qml/DefaultApplicationsPage.qml
)
target_link_libraries(
    qindaqt_settings_default_apps_qml
    PRIVATE
        qindaqt_settings_default_apps QindaQt::ApplicationCatalog
        QindaQt::TokensQml
        Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2
)
set_target_properties(
    qindaqt_settings_default_apps_qml PROPERTIES
    CXX_EXTENSIONS OFF
    INSTALL_RPATH "$ORIGIN/../../Tokens;$ORIGIN/../../Controls"
)
qindaqt_enable_warnings(qindaqt_settings_default_apps_qml)

install(
    TARGETS qindaqt_settings_default_apps_qml qindaqt_settings_default_apps_qmlplugin
    RUNTIME DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/DefaultApplications"
        COMPONENT SettingsAppearanceRuntime
    LIBRARY DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/DefaultApplications"
        COMPONENT SettingsAppearanceRuntime
    ARCHIVE DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/DefaultApplications"
        COMPONENT SettingsAppearanceRuntime
)
install(
    FILES
        "${QINDAQT_DEFAULT_APPS_SETTINGS_QML_OUTPUT_DIRECTORY}/qmldir"
        "${QINDAQT_DEFAULT_APPS_SETTINGS_QML_OUTPUT_DIRECTORY}/qindaqt_settings_default_apps.qmltypes"
        qml/DefaultApplicationsPage.qml
    DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/DefaultApplications"
    COMPONENT SettingsAppearanceRuntime
)

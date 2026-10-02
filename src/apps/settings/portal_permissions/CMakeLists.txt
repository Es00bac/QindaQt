# SPDX-License-Identifier: GPL-3.0-or-later
qt_add_library(qindaqt_settings_portal_permissions STATIC)
target_sources(qindaqt_settings_portal_permissions PRIVATE portal_permissions_model.cpp
    PUBLIC FILE_SET HEADERS BASE_DIRS include FILES
    include/qindaqt/apps/settings_portal_permissions/portal_permissions_model.h)
target_link_libraries(qindaqt_settings_portal_permissions PUBLIC Qt6::Core Qt6::DBus)
target_compile_features(qindaqt_settings_portal_permissions PUBLIC cxx_std_20)
set_target_properties(qindaqt_settings_portal_permissions PROPERTIES POSITION_INDEPENDENT_CODE ON CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_settings_portal_permissions)
set(module_dir "${CMAKE_BINARY_DIR}/qml/QindaQt/SettingsApp/PortalPermissions")
qt_add_library(qindaqt_settings_portal_permissions_qml STATIC)
qt_add_qml_module(qindaqt_settings_portal_permissions_qml
    URI QindaQt.SettingsApp.PortalPermissions VERSION 1.0
    CLASS_NAME QindaQtSettingsPortalPermissionsPlugin OUTPUT_DIRECTORY "${module_dir}"
    SOURCES portal_permissions_route_composition.h
    QML_FILES qml/PortalPermissionsPage.qml)
target_link_libraries(qindaqt_settings_portal_permissions_qml PRIVATE
    qindaqt_settings_portal_permissions QindaQt::TokensQml qindaqt_controls_qml
    Qt6::Qml Qt6::Quick Qt6::QuickControls2)
set_target_properties(qindaqt_settings_portal_permissions_qml PROPERTIES CXX_EXTENSIONS OFF
    INSTALL_RPATH "$ORIGIN/../../Tokens;$ORIGIN/../../Controls")
qindaqt_enable_warnings(qindaqt_settings_portal_permissions_qml)
install(TARGETS qindaqt_settings_portal_permissions_qml qindaqt_settings_portal_permissions_qmlplugin
    RUNTIME DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/PortalPermissions" COMPONENT SettingsAppearanceRuntime
    LIBRARY DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/PortalPermissions" COMPONENT SettingsAppearanceRuntime
    ARCHIVE DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/PortalPermissions" COMPONENT SettingsAppearanceRuntime)
install(FILES "${module_dir}/qmldir" "${module_dir}/qindaqt_settings_portal_permissions_qml.qmltypes" qml/PortalPermissionsPage.qml
    DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/PortalPermissions" COMPONENT SettingsAppearanceRuntime)

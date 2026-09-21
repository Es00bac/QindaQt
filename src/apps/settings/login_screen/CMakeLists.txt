# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_library(qindaqt_settings_login_screen STATIC)
target_sources(
    qindaqt_settings_login_screen
    PRIVATE
        login_screen_settings_model.cpp
        sddm_config_store.cpp
        sddm_discovery.cpp
        sddm_owned_config.cpp
        pkexec_sddm_config_writer.h
        pkexec_sddm_config_writer.cpp
        polkit_authority_probe.h
        polkit_authority_probe.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/apps/settings_login_screen/login_screen_settings_model.h
            include/qindaqt/apps/settings_login_screen/sddm_config_store.h
            include/qindaqt/apps/settings_login_screen/sddm_discovery.h
            include/qindaqt/apps/settings_login_screen/sddm_owned_config.h
)
target_link_libraries(
    qindaqt_settings_login_screen
    PUBLIC Qt6::Core
    # polkit's authority lives on the system bus; no consumer of this
    # library sees a bus type (same privacy rule as the Date & time route).
    PRIVATE Qt6::DBus
)
target_compile_features(qindaqt_settings_login_screen PUBLIC cxx_std_20)
set_target_properties(
    qindaqt_settings_login_screen
    PROPERTIES CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON
)
qindaqt_enable_warnings(qindaqt_settings_login_screen)

install(
    TARGETS qindaqt_settings_login_screen
    EXPORT QindaQtTargets
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

set(QINDAQT_LOGIN_SCREEN_SETTINGS_QML_OUTPUT_DIRECTORY
    "${CMAKE_BINARY_DIR}/qml/QindaQt/SettingsApp/LoginScreen")
qt_add_library(qindaqt_settings_login_screen_qml STATIC)
qt_add_qml_module(
    qindaqt_settings_login_screen_qml
    URI QindaQt.SettingsApp.LoginScreen
    VERSION 1.0
    CLASS_NAME QindaQtSettingsLoginScreenPlugin
    OUTPUT_DIRECTORY "${QINDAQT_LOGIN_SCREEN_SETTINGS_QML_OUTPUT_DIRECTORY}"
    TYPEINFO qindaqt_settings_login_screen.qmltypes
    SOURCES
        login_screen_route_composition.h
        login_screen_route_composition.cpp
    QML_FILES qml/LoginScreenPage.qml
)
target_compile_definitions(
    qindaqt_settings_login_screen_qml
    PRIVATE
        QINDAQT_SDDM_HELPER_PATH="${KDE_INSTALL_FULL_LIBEXECDIR}/qindaqt-sddm-config-helper"
)
target_link_libraries(
    qindaqt_settings_login_screen_qml
    PRIVATE
        qindaqt_settings_login_screen QindaQt::TokensQml
        Qt6::Core Qt6::DBus Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2
)
set_target_properties(
    qindaqt_settings_login_screen_qml PROPERTIES
    CXX_EXTENSIONS OFF
    INSTALL_RPATH "$ORIGIN/../../Tokens;$ORIGIN/../../Controls"
)
qindaqt_enable_warnings(qindaqt_settings_login_screen_qml)

install(
    TARGETS qindaqt_settings_login_screen_qml qindaqt_settings_login_screen_qmlplugin
    RUNTIME DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/LoginScreen"
        COMPONENT SettingsAppearanceRuntime
    LIBRARY DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/LoginScreen"
        COMPONENT SettingsAppearanceRuntime
    ARCHIVE DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/LoginScreen"
        COMPONENT SettingsAppearanceRuntime
)
install(
    FILES
        "${QINDAQT_LOGIN_SCREEN_SETTINGS_QML_OUTPUT_DIRECTORY}/qmldir"
        "${QINDAQT_LOGIN_SCREEN_SETTINGS_QML_OUTPUT_DIRECTORY}/qindaqt_settings_login_screen.qmltypes"
        qml/LoginScreenPage.qml
    DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/LoginScreen"
    COMPONENT SettingsAppearanceRuntime
)

# ADR-0225: the privileged half of the route. The Settings process never
# writes /etc itself; this helper runs only through pkexec, gated by the
# org.qindaqt.settings.loginscreen.configure action installed below.
qt_add_executable(qindaqt-sddm-config-helper helper/sddm_config_helper_main.cpp)
target_link_libraries(
    qindaqt-sddm-config-helper
    PRIVATE qindaqt_settings_login_screen Qt6::Core
)
qindaqt_enable_warnings(qindaqt-sddm-config-helper)
install(
    TARGETS qindaqt-sddm-config-helper
    RUNTIME DESTINATION "${KDE_INSTALL_LIBEXECDIR}"
    COMPONENT SettingsAppearanceRuntime
)

set(QINDAQT_SDDM_HELPER_INSTALL_PATH
    "${KDE_INSTALL_FULL_LIBEXECDIR}/qindaqt-sddm-config-helper")
configure_file(
    helper/org.qindaqt.settings.loginscreen.policy.in
    "${CMAKE_CURRENT_BINARY_DIR}/org.qindaqt.settings.loginscreen.policy"
    @ONLY
)
install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/org.qindaqt.settings.loginscreen.policy"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/polkit-1/actions"
    COMPONENT SettingsAppearanceRuntime
)

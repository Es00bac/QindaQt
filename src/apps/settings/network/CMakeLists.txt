# SPDX-License-Identifier: GPL-3.0-or-later

# Public-Network1 consumer model for the first-party Settings Network route.
qt_add_library(qindaqt_settings_network STATIC)
target_sources(
    qindaqt_settings_network
    PRIVATE
        network_secret_agent_presence.cpp
        network_settings_actions.cpp
        network_settings_model.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/apps/settings_network/network_settings_model.h
            include/qindaqt/apps/settings_network/network_secret_agent_presence.h
)
target_link_libraries(
    qindaqt_settings_network
    PUBLIC QindaQt::NetworkClient QindaQt::NetworkModel
           QindaQt::NetworkProtocol Qt6::Core Qt6::DBus
)
target_compile_features(qindaqt_settings_network PUBLIC cxx_std_20)
set_target_properties(
    qindaqt_settings_network
    PROPERTIES CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON
)
qindaqt_enable_warnings(qindaqt_settings_network)

set(QINDAQT_NETWORK_SETTINGS_QML_OUTPUT_DIRECTORY
    "${CMAKE_BINARY_DIR}/qml/QindaQt/SettingsApp/Network")
qt_add_library(qindaqt_settings_network_qml SHARED)
qt_add_qml_module(
    qindaqt_settings_network_qml
    URI QindaQt.SettingsApp.Network
    VERSION 1.0
    CLASS_NAME QindaQtSettingsNetworkPlugin
    OUTPUT_DIRECTORY "${QINDAQT_NETWORK_SETTINGS_QML_OUTPUT_DIRECTORY}"
    TYPEINFO qindaqt_settings_network.qmltypes
    QML_FILES
        qml/NetworkAccessPointSection.qml
        qml/NetworkDeviceSection.qml
        qml/NetworkPage.qml
        qml/NetworkRadioSection.qml
        qml/NetworkSavedSection.qml
)
target_link_libraries(
    qindaqt_settings_network_qml
    PRIVATE QindaQt::TokensQml Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick
            Qt6::QuickControls2
)
set_target_properties(
    qindaqt_settings_network_qml
    PROPERTIES
        CXX_EXTENSIONS OFF
        INSTALL_RPATH "$ORIGIN/../../Tokens;$ORIGIN/../../Controls"
)
qindaqt_enable_warnings(qindaqt_settings_network_qml)

install(
    TARGETS qindaqt_settings_network_qml qindaqt_settings_network_qmlplugin
    RUNTIME DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Network"
        COMPONENT SettingsAppearanceRuntime
    LIBRARY DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Network"
        COMPONENT SettingsAppearanceRuntime
    ARCHIVE DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Network"
        COMPONENT SettingsAppearanceRuntime
)
install(
    FILES
        "${QINDAQT_NETWORK_SETTINGS_QML_OUTPUT_DIRECTORY}/qmldir"
        "${QINDAQT_NETWORK_SETTINGS_QML_OUTPUT_DIRECTORY}/qindaqt_settings_network.qmltypes"
    DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Network"
    COMPONENT SettingsAppearanceRuntime
)
install(
    FILES
        qml/NetworkAccessPointSection.qml
        qml/NetworkDeviceSection.qml
        qml/NetworkPage.qml
        qml/NetworkRadioSection.qml
        qml/NetworkSavedSection.qml
    DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Network"
    COMPONENT SettingsAppearanceRuntime
)

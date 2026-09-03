# SPDX-License-Identifier: GPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Core DBus Gui Qml Quick QuickControls2)

qt_add_library(qindaqt_network_secret_agent STATIC)
add_library(QindaQt::NetworkSecretAgent ALIAS qindaqt_network_secret_agent)
target_sources(
    qindaqt_network_secret_agent
    PRIVATE
        src/dbus_connection_authority.cpp
        src/dbus_connection_authority_p.h
        src/qml_prompt_presenter.cpp
        src/resident_secret_agent.cpp
        src/secret_agent_controller.cpp
        src/secret_agent_object.cpp
        src/secret_agent_object_p.h
        src/secret_agent_types.cpp
        src/secret_request_policy.cpp
        src/secret_request_policy_p.h
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/services/network_secret_agent/connection_authority.h
            include/qindaqt/services/network_secret_agent/prompt_port.h
            include/qindaqt/services/network_secret_agent/qml_prompt_presenter.h
            include/qindaqt/services/network_secret_agent/resident_secret_agent.h
            include/qindaqt/services/network_secret_agent/secret_agent_controller.h
            include/qindaqt/services/network_secret_agent/secret_agent_types.h
)
qt_add_resources(
    qindaqt_network_secret_agent network_secret_agent_qml
    PREFIX "/qindaqt/network-secret-agent"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/qml"
    FILES qml/NetworkSecretPrompt.qml
)
target_link_libraries(
    qindaqt_network_secret_agent
    PUBLIC Qt6::Core Qt6::DBus
    PRIVATE QindaQt::TokensQml Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2
)
target_compile_features(qindaqt_network_secret_agent PUBLIC cxx_std_20)
target_compile_definitions(qindaqt_network_secret_agent PRIVATE QT_NO_KEYWORDS)
set_target_properties(
    qindaqt_network_secret_agent
    PROPERTIES CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON
)
qindaqt_enable_warnings(qindaqt_network_secret_agent)

qt_add_executable(qindaqt-network-secret-agent app/main.cpp)
target_link_libraries(
    qindaqt-network-secret-agent
    PRIVATE
        QindaQt::NetworkSecretAgent
        QindaQt::DesignTokens
        QindaQt::TokensQml
        qindaqt_themes
        qindaqt_controls_qml
        Qt6::Core Qt6::DBus Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2
)
target_compile_definitions(
    qindaqt-network-secret-agent
    PRIVATE
        QINDAQT_BUILD_EXECUTABLE_PATH="${CMAKE_CURRENT_BINARY_DIR}/qindaqt-network-secret-agent${CMAKE_EXECUTABLE_SUFFIX}"
        QINDAQT_BUILD_QML_IMPORT_PATH="${CMAKE_BINARY_DIR}/qml"
        QINDAQT_BUILD_THEME_PATH="${PROJECT_SOURCE_DIR}/data/themes/qinda-light.json"
        QINDAQT_INSTALL_QML_PATH="../${QT6_INSTALL_QML}"
        QINDAQT_INSTALL_THEME_PATH="../${CMAKE_INSTALL_DATADIR}/qindaqt/themes/qinda-light.json"
)
set_target_properties(
    qindaqt-network-secret-agent
    PROPERTIES
        CXX_EXTENSIONS OFF
        INSTALL_RPATH "$ORIGIN/../${QT6_INSTALL_QML}/QindaQt/Tokens;$ORIGIN/../${QT6_INSTALL_QML}/QindaQt/Controls"
)
add_dependencies(
    qindaqt-network-secret-agent
    qindaqt_tokens_qml qindaqt_tokens_qmlplugin
    qindaqt_controls_qml qindaqt_controls_qmlplugin
)
qindaqt_enable_warnings(qindaqt-network-secret-agent)

install(
    TARGETS qindaqt-network-secret-agent
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    COMPONENT QindaQtNetworkSecretAgent
)

# The agent is an independently deployable process. Its component therefore
# carries the two QML modules and one theme it needs, without pulling in the
# Settings Appearance component's unrelated route plugins.
install(
    TARGETS qindaqt_tokens_qml qindaqt_tokens_qmlplugin
    RUNTIME DESTINATION "${QT6_INSTALL_QML}/QindaQt/Tokens"
        COMPONENT QindaQtNetworkSecretAgent
    LIBRARY DESTINATION "${QT6_INSTALL_QML}/QindaQt/Tokens"
        COMPONENT QindaQtNetworkSecretAgent
)
install(
    FILES
        "${CMAKE_BINARY_DIR}/qml/QindaQt/Tokens/qmldir"
        "${CMAKE_BINARY_DIR}/qml/QindaQt/Tokens/qindaqt_tokens.qmltypes"
    DESTINATION "${QT6_INSTALL_QML}/QindaQt/Tokens"
    COMPONENT QindaQtNetworkSecretAgent
)
install(
    TARGETS qindaqt_controls_qml qindaqt_controls_qmlplugin
    RUNTIME DESTINATION "${QT6_INSTALL_QML}/QindaQt/Controls"
        COMPONENT QindaQtNetworkSecretAgent
    LIBRARY DESTINATION "${QT6_INSTALL_QML}/QindaQt/Controls"
        COMPONENT QindaQtNetworkSecretAgent
)
install(
    FILES
        "${CMAKE_BINARY_DIR}/qml/QindaQt/Controls/qmldir"
        "${CMAKE_BINARY_DIR}/qml/QindaQt/Controls/qindaqt_controls.qmltypes"
    DESTINATION "${QT6_INSTALL_QML}/QindaQt/Controls"
    COMPONENT QindaQtNetworkSecretAgent
)
install(
    DIRECTORY "${PROJECT_SOURCE_DIR}/src/controls/qml/"
    DESTINATION "${QT6_INSTALL_QML}/QindaQt/Controls/qml"
    COMPONENT QindaQtNetworkSecretAgent
    FILES_MATCHING PATTERN "*.qml"
)
install(
    FILES "${PROJECT_SOURCE_DIR}/data/themes/qinda-light.json"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/qindaqt/themes"
    COMPONENT QindaQtNetworkSecretAgent
)

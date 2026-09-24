# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_executable(
    qindaqt_network_secret_agent_controller_tests
    tst_secret_agent_controller.cpp
)
target_link_libraries(
    qindaqt_network_secret_agent_controller_tests
    PRIVATE QindaQt::NetworkSecretAgent Qt6::Test
)
qindaqt_enable_warnings(qindaqt_network_secret_agent_controller_tests)
add_test(
    NAME qindaqt.network-secret-agent-controller
    COMMAND qindaqt_network_secret_agent_controller_tests
)
set_tests_properties(
    qindaqt.network-secret-agent-controller
    PROPERTIES LABELS "service;network;secret-agent" TIMEOUT 20
)

qt_add_executable(
    qindaqt_network_secret_agent_prompt_tests
    tst_network_secret_prompt.cpp
)
target_link_libraries(
    qindaqt_network_secret_agent_prompt_tests
    PRIVATE
        QindaQt::NetworkSecretAgent
        QindaQt::DesignTokens
        QindaQt::TokensQml
        qindaqt_themes
        qindaqt_controls_qml
        Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Test
)
target_compile_definitions(
    qindaqt_network_secret_agent_prompt_tests
    PRIVATE
        QINDAQT_TEST_QML_IMPORT_PATH="${CMAKE_BINARY_DIR}/qml"
        QINDAQT_TEST_THEME_PATH="${PROJECT_SOURCE_DIR}/data/themes/qinda-light.json"
)
add_dependencies(
    qindaqt_network_secret_agent_prompt_tests
    qindaqt_tokens_qml qindaqt_tokens_qmlplugin
    qindaqt_controls_qml qindaqt_controls_qmlplugin
)
qindaqt_enable_warnings(qindaqt_network_secret_agent_prompt_tests)
add_test(
    NAME qindaqt.network-secret-agent-prompt
    COMMAND qindaqt_network_secret_agent_prompt_tests
)
set_tests_properties(
    qindaqt.network-secret-agent-prompt
    PROPERTIES
        LABELS "service;network;secret-agent;offscreen;accessibility"
        TIMEOUT 30
        ENVIRONMENT
            "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_FATAL_WARNINGS=1;DISPLAY=;WAYLAND_DISPLAY=;DBUS_SESSION_BUS_ADDRESS=;DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent"
)

# Shared private-bus fixture: disposable broker plus fake NetworkManager.
qt_add_library(
    qindaqt_network_secret_agent_test_bus STATIC
    private_network_manager_bus.cpp
    private_network_manager_bus.h
)
target_link_libraries(
    qindaqt_network_secret_agent_test_bus
    PUBLIC QindaQt::NetworkSecretAgent Qt6::DBus
)
qindaqt_enable_warnings(qindaqt_network_secret_agent_test_bus)

qt_add_executable(
    qindaqt_network_secret_agent_dbus_tests
    tst_network_secret_agent_dbus.cpp
)
target_link_libraries(
    qindaqt_network_secret_agent_dbus_tests
    PRIVATE qindaqt_network_secret_agent_test_bus Qt6::Test
)
qindaqt_enable_warnings(qindaqt_network_secret_agent_dbus_tests)
add_test(
    NAME qindaqt.network-secret-agent-dbus
    COMMAND qindaqt_network_secret_agent_dbus_tests
)
set_tests_properties(
    qindaqt.network-secret-agent-dbus
    PROPERTIES
        LABELS "service;network;secret-agent;private-bus"
        TIMEOUT 30
        ENVIRONMENT
            "DBUS_SESSION_BUS_ADDRESS=;DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent;TMPDIR=${CMAKE_CURRENT_BINARY_DIR}"
)

qt_add_executable(
    qindaqt_network_secret_agent_ip_config_tests
    tst_network_secret_agent_ip_config.cpp
)
target_include_directories(
    qindaqt_network_secret_agent_ip_config_tests
    PRIVATE ${PROJECT_SOURCE_DIR}/src/services/network_secret_agent/src
)
target_link_libraries(
    qindaqt_network_secret_agent_ip_config_tests
    PRIVATE qindaqt_network_secret_agent_test_bus Qt6::Test
)
qindaqt_enable_warnings(qindaqt_network_secret_agent_ip_config_tests)
add_test(
    NAME qindaqt.network-secret-agent-ip-config
    COMMAND qindaqt_network_secret_agent_ip_config_tests
)
set_tests_properties(
    qindaqt.network-secret-agent-ip-config
    PROPERTIES
        LABELS "service;network;secret-agent;private-bus"
        TIMEOUT 60
        ENVIRONMENT
            "DBUS_SESSION_BUS_ADDRESS=;DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent;TMPDIR=${CMAKE_CURRENT_BINARY_DIR}"
)

# Drives the built executable (not a library double) so the process-lifetime
# configuration in app/main.cpp is what is proven.
qt_add_executable(
    qindaqt_network_secret_agent_lifetime_tests
    tst_network_secret_agent_lifetime.cpp
)
target_link_libraries(
    qindaqt_network_secret_agent_lifetime_tests
    PRIVATE qindaqt_network_secret_agent_test_bus Qt6::Test
)
target_compile_definitions(
    qindaqt_network_secret_agent_lifetime_tests
    PRIVATE
        QINDAQT_NETWORK_SECRET_AGENT_EXECUTABLE="$<TARGET_FILE:qindaqt-network-secret-agent>"
)
add_dependencies(
    qindaqt_network_secret_agent_lifetime_tests qindaqt-network-secret-agent
)
qindaqt_enable_warnings(qindaqt_network_secret_agent_lifetime_tests)
add_test(
    NAME qindaqt.network-secret-agent-lifetime
    COMMAND qindaqt_network_secret_agent_lifetime_tests
)
set_tests_properties(
    qindaqt.network-secret-agent-lifetime
    PROPERTIES
        LABELS "service;network;secret-agent;private-bus;offscreen"
        TIMEOUT 60
        ENVIRONMENT
            "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;DISPLAY=;WAYLAND_DISPLAY=;DBUS_SESSION_BUS_ADDRESS=;DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent;TMPDIR=${CMAKE_CURRENT_BINARY_DIR}"
)

add_test(
    NAME qindaqt.network-secret-agent-installed
    COMMAND
        "${CMAKE_COMMAND}"
        -DQINDAQT_BUILD_DIR=${CMAKE_BINARY_DIR}
        -DQINDAQT_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/installed
        -DQINDAQT_EXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_installed_agent.cmake
)
set_tests_properties(
    qindaqt.network-secret-agent-installed
    PROPERTIES
        LABELS "service;network;secret-agent;installed;real-relocation"
        TIMEOUT 30
)

add_test(
    NAME qindaqt.network-secret-agent-boundary
    COMMAND "${CMAKE_COMMAND}" -DSOURCE_ROOT=${PROJECT_SOURCE_DIR}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/check_boundary.cmake
)
set_tests_properties(
    qindaqt.network-secret-agent-boundary
    PROPERTIES LABELS "service;network;secret-agent;boundary"
)

add_test(
    NAME qindaqt.network-secret-agent-boundary-poison
    COMMAND
        "${CMAKE_COMMAND}"
        -DPOISON_ROOT=${CMAKE_CURRENT_BINARY_DIR}/boundary-poison
        -DCHECK_SCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/check_boundary.cmake
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_boundary_negative.cmake
)
set_tests_properties(
    qindaqt.network-secret-agent-boundary-poison
    PROPERTIES LABELS "service;network;secret-agent;boundary;poison"
)

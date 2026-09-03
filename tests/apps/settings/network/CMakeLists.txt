# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_executable(
    qindaqt_network_settings_model_tests
    tst_network_settings_model.cpp
    network_settings_test_support.h
)
target_include_directories(
    qindaqt_network_settings_model_tests PRIVATE "${PROJECT_SOURCE_DIR}"
)
target_link_libraries(
    qindaqt_network_settings_model_tests
    PRIVATE qindaqt_settings_network Qt6::Test
)
qindaqt_enable_warnings(qindaqt_network_settings_model_tests)
add_test(
    NAME qindaqt.network-settings-model
    COMMAND qindaqt_network_settings_model_tests
)

qt_add_executable(
    qindaqt_network_settings_model_adversarial_tests
    tst_network_settings_model_adversarial.cpp
    network_settings_test_support.h
)
target_include_directories(
    qindaqt_network_settings_model_adversarial_tests
    PRIVATE "${PROJECT_SOURCE_DIR}"
)
target_link_libraries(
    qindaqt_network_settings_model_adversarial_tests
    PRIVATE qindaqt_settings_network Qt6::Test
)
qindaqt_enable_warnings(qindaqt_network_settings_model_adversarial_tests)
add_test(
    NAME qindaqt.network-settings-model-adversarial
    COMMAND qindaqt_network_settings_model_adversarial_tests
)

qt_add_executable(
    qindaqt_network_page_tests
    tst_network_page.cpp
    stub_network_settings_model.h
)
target_include_directories(
    qindaqt_network_page_tests PRIVATE "${PROJECT_SOURCE_DIR}"
)
target_compile_definitions(
    qindaqt_network_page_tests PRIVATE
    QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
    QINDAQT_QML_IMPORT_PATH="${CMAKE_BINARY_DIR}/qml"
    QINDAQT_NETWORK_PAGE_QML_PATH="${PROJECT_SOURCE_DIR}/src/apps/settings/network/qml/NetworkPage.qml"
)
target_link_libraries(
    qindaqt_network_page_tests
    PRIVATE qindaqt_settings_network qindaqt_settings_appearance
            QindaQt::TokensQml qindaqt_themes Qt6::Quick
            Qt6::QuickControls2 Qt6::Test
)
qindaqt_enable_warnings(qindaqt_network_page_tests)
add_dependencies(
    qindaqt_network_page_tests
    qindaqt_tokens_qmlplugin
    qindaqt_controls_qmlplugin
    qindaqt_settings_network_qmlplugin
)
add_test(NAME qindaqt.network-page COMMAND qindaqt_network_page_tests)
set_tests_properties(
    qindaqt.network-page PROPERTIES
    LABELS "offscreen;settings;network;accessibility"
    ENVIRONMENT
        "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_LINUX_ACCESSIBILITY_ALWAYS_ON=1;QT_FATAL_WARNINGS=1"
)

add_test(
    NAME qindaqt.network-settings-boundary
    COMMAND "${CMAKE_COMMAND}" -DSOURCE_ROOT=${PROJECT_SOURCE_DIR}
            -P "${CMAKE_CURRENT_SOURCE_DIR}/check_boundary.cmake"
)

qt_add_executable(
    qindaqt_network_secret_agent_presence_tests
    tst_network_secret_agent_presence.cpp
)
target_link_libraries(
    qindaqt_network_secret_agent_presence_tests
    PRIVATE qindaqt_settings_network Qt6::DBus Qt6::Test
)
qindaqt_enable_warnings(qindaqt_network_secret_agent_presence_tests)
add_test(
    NAME qindaqt.settings-network-secret-agent-presence
    COMMAND qindaqt_network_secret_agent_presence_tests
)
set_tests_properties(
    qindaqt.settings-network-secret-agent-presence
    PROPERTIES
        LABELS "settings;network;secret-agent;private-bus"
        TIMEOUT 20
        ENVIRONMENT "DBUS_SESSION_BUS_ADDRESS=;DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent"
)
set_tests_properties(
    qindaqt.network-settings-boundary PROPERTIES
    LABELS "settings;network;boundary"
)

add_test(
    NAME qindaqt.network-settings-boundary-poison
    COMMAND "${CMAKE_COMMAND}"
            -DBINARY_ROOT=${CMAKE_BINARY_DIR}
            -DPOISON_ROOT=${CMAKE_CURRENT_BINARY_DIR}/boundary-poison
            -DCHECK_SCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/check_boundary.cmake
            -P "${CMAKE_CURRENT_SOURCE_DIR}/check_boundary_negative.cmake"
)
set_tests_properties(
    qindaqt.network-settings-boundary-poison PROPERTIES
    LABELS "settings;network;boundary;poison"
)

# SPDX-License-Identifier: GPL-3.0-or-later

function(qindaqt_add_font_discovery_test target source test_name)
    qt_add_executable(${target} ${source})
    target_link_libraries(
        ${target}
        PRIVATE QindaQt::FontDiscovery Qt6::Test
    )
    target_compile_definitions(
        ${target}
        PRIVATE QINDAQT_FONT_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
    )
    target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    qindaqt_enable_warnings(${target})
    add_test(NAME ${test_name} COMMAND ${target})
    set_tests_properties(${test_name} PROPERTIES LABELS "service;fonts;discovery")
endfunction()

qindaqt_add_font_discovery_test(
    qindaqt_font_discovery_tests
    tst_font_discovery.cpp
    qindaqt.font-discovery
)
qindaqt_add_font_discovery_test(
    qindaqt_font_discovery_hostile_tests
    tst_font_discovery_hostile.cpp
    qindaqt.font-discovery-hostile
)
qindaqt_add_font_discovery_test(
    qindaqt_font_discovery_bounds_tests
    tst_font_discovery_bounds.cpp
    qindaqt.font-discovery-bounds
)

# The hostile and bounds rows stage symlink loops, unreadable files, and
# thousands of font entries (P2-1); bound their runtime so a regression hangs
# fail the row instead of blocking the suite.
set_tests_properties(qindaqt.font-discovery-hostile PROPERTIES TIMEOUT 120)
set_tests_properties(qindaqt.font-discovery-bounds PROPERTIES TIMEOUT 300)

find_program(QINDAQT_DBUS_DAEMON_EXECUTABLE NAMES dbus-daemon REQUIRED)

qt_add_executable(qindaqt_font_session_bootstrap_tests tst_font_session_bootstrap.cpp)
target_link_libraries(
    qindaqt_font_session_bootstrap_tests
    PRIVATE QindaQt::FontDiscovery QindaQt::SettingsProtocol Qt6::Test Qt6::DBus Qt6::Gui
)
target_compile_definitions(
    qindaqt_font_session_bootstrap_tests
    PRIVATE
        QINDAQT_FONT_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
        QINDAQT_DBUS_DAEMON_EXECUTABLE="${QINDAQT_DBUS_DAEMON_EXECUTABLE}"
        QINDAQT_TEST_SETTINGS_SERVICE_EXECUTABLE="$<TARGET_FILE:qindaqt-settings-service>"
        QINDAQT_TEST_SETTINGS_SCHEMA_DIR="${CMAKE_SOURCE_DIR}/data/settings"
)
qindaqt_enable_warnings(qindaqt_font_session_bootstrap_tests)
# The real-service row executes the production settings-service binary on a
# private bus, so the executable must exist before the test runs.
add_dependencies(qindaqt_font_session_bootstrap_tests qindaqt-settings-service)
add_test(NAME qindaqt.font-session-bootstrap COMMAND qindaqt_font_session_bootstrap_tests)
set_tests_properties(
    qindaqt.font-session-bootstrap
    PROPERTIES LABELS "service;fonts;settings1" TIMEOUT 300
)

add_test(
    NAME qindaqt.font-application-bootstrap-wiring
    COMMAND
        "${CMAKE_COMMAND}"
        "-DSOURCE_ROOT=${CMAKE_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/check_application_bootstrap.cmake"
)
set_tests_properties(
    qindaqt.font-application-bootstrap-wiring
    PROPERTIES LABELS "service;fonts;policy"
)

add_test(
    NAME qindaqt.font-discovery-boundary
    COMMAND
        "${CMAKE_COMMAND}"
        "-DSOURCE_ROOT=${CMAKE_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/check_boundary.cmake"
)
set_tests_properties(qindaqt.font-discovery-boundary PROPERTIES LABELS "service;fonts;policy")

add_test(
    NAME qindaqt.font-discovery-installed-consumer
    COMMAND
        "${CMAKE_COMMAND}"
        "-DQINDAQT_CMAKE=${CMAKE_COMMAND}"
        "-DQINDAQT_BUILD_DIRECTORY=${CMAKE_BINARY_DIR}"
        "-DQINDAQT_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/installed-font-discovery-stage"
        "-DQINDAQT_FONT_PREFERENCES_SOURCE_DIRECTORY=${CMAKE_SOURCE_DIR}/src/services/font_preferences"
        "-DQINDAQT_FONT_DISCOVERY_SOURCE_DIRECTORY=${CMAKE_SOURCE_DIR}/src/services/font_discovery"
        "-DQINDAQT_INSTALL_INCLUDEDIR=${CMAKE_INSTALL_INCLUDEDIR}"
        "-DQINDAQT_FONT_PREFERENCES_LIBRARY=$<TARGET_FILE:qindaqt_font_preferences>"
        "-DQINDAQT_FONT_DISCOVERY_LIBRARY=$<TARGET_FILE:qindaqt_font_discovery>"
        "-DQINDAQT_SETTINGS_CLIENT_LIBRARY=$<TARGET_FILE:qindaqt_settings_client>"
        "-DQINDAQT_SETTINGS_PROTOCOL_LIBRARY=$<TARGET_FILE:qindaqt_settings_protocol>"
        "-DQINDAQT_QT6_DIR=${Qt6_DIR}"
        "-DQINDAQT_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
        "-DQINDAQT_CONSUMER_SOURCE_DIRECTORY=${CMAKE_CURRENT_SOURCE_DIR}/installed_consumer"
        "-DQINDAQT_CONSUMER_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/installed_consumer/installed_font_discovery_consumer.cpp"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_installed_font_discovery_consumer.cmake"
)
set_tests_properties(qindaqt.font-discovery-installed-consumer PROPERTIES LABELS "service;fonts;installed")

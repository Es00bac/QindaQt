# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_executable(
    qindaqt_session_token_child_helper
    session_token_child_helper.cpp
)
target_link_libraries(
    qindaqt_session_token_child_helper
    PRIVATE QindaQt::NotificationPresentationProtocol Qt6::Core Qt6::DBus
)
qindaqt_enable_warnings(qindaqt_session_token_child_helper)

qt_add_executable(
    qindaqt_session_parent_death_helper
    session_parent_death_helper.cpp
)
target_link_libraries(
    qindaqt_session_parent_death_helper
    PRIVATE QindaQt::SessionSupervisorSupport Qt6::Core
)
qindaqt_enable_warnings(qindaqt_session_parent_death_helper)

qt_add_executable(
    qindaqt_session_plain_child_helper
    session_plain_child_helper.cpp
)
target_link_libraries(qindaqt_session_plain_child_helper PRIVATE Qt6::Core)
qindaqt_enable_warnings(qindaqt_session_plain_child_helper)

qt_add_executable(
    qindaqt_session_supervisor_tests
    tst_session_process_supervisor.cpp
)
target_link_libraries(
    qindaqt_session_supervisor_tests
    PRIVATE QindaQt::SessionSupervisorSupport Qt6::Test
)
target_compile_definitions(
    qindaqt_session_supervisor_tests
    PRIVATE
        QINDAQT_SESSION_TOKEN_CHILD_HELPER="$<TARGET_FILE:qindaqt_session_token_child_helper>"
        QINDAQT_SESSION_PARENT_DEATH_HELPER="$<TARGET_FILE:qindaqt_session_parent_death_helper>"
        QINDAQT_SESSION_PLAIN_CHILD_HELPER="$<TARGET_FILE:qindaqt_session_plain_child_helper>"
        QINDAQT_SESSION1_XML="${PROJECT_SOURCE_DIR}/src/session_supervisor/org.qindaqt.Session1.xml"
)
add_dependencies(
    qindaqt_session_supervisor_tests
    qindaqt_session_token_child_helper
    qindaqt_session_parent_death_helper
    qindaqt_session_plain_child_helper
)
qindaqt_enable_warnings(qindaqt_session_supervisor_tests)
add_test(
    NAME qindaqt.session-supervisor
    COMMAND dbus-run-session -- $<TARGET_FILE:qindaqt_session_supervisor_tests>
)
set_tests_properties(
    qindaqt.session-supervisor PROPERTIES
        TIMEOUT 120
        LABELS "session;service;dbus"
        ENVIRONMENT "DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent"
)

qt_add_executable(qindaqt_polkit_agent_selection_tests tst_polkit_agent_selection.cpp)
target_link_libraries(qindaqt_polkit_agent_selection_tests
    PRIVATE QindaQt::SessionSupervisorSupport Qt6::Core Qt6::Test)
qindaqt_enable_warnings(qindaqt_polkit_agent_selection_tests)
add_test(NAME qindaqt.session-polkit-agent-selection
    COMMAND qindaqt_polkit_agent_selection_tests)
set_tests_properties(qindaqt.session-polkit-agent-selection PROPERTIES
    LABELS "session;model")

qt_add_executable(qindaqt_session_activation_publisher session_activation_publisher.cpp)
target_include_directories(qindaqt_session_activation_publisher PRIVATE "${PROJECT_SOURCE_DIR}")
target_link_libraries(qindaqt_session_activation_publisher PRIVATE QindaQt::SessionSupervisorSupport)
qindaqt_enable_warnings(qindaqt_session_activation_publisher)
qt_add_executable(qindaqt_activation_environment_tests tst_activation_environment.cpp)
target_link_libraries(qindaqt_activation_environment_tests PRIVATE Qt6::Core Qt6::DBus Qt6::Test)
target_compile_definitions(qindaqt_activation_environment_tests PRIVATE
    QINDAQT_ACTIVATION_PUBLISHER="$<TARGET_FILE:qindaqt_session_activation_publisher>")
add_dependencies(qindaqt_activation_environment_tests qindaqt_session_activation_publisher)
qindaqt_enable_warnings(qindaqt_activation_environment_tests)
add_test(NAME qindaqt.session-activation-environment
    COMMAND dbus-run-session -- $<TARGET_FILE:qindaqt_activation_environment_tests>)
set_tests_properties(qindaqt.session-activation-environment PROPERTIES TIMEOUT 15 LABELS "session;dbus")

qt_add_executable(
    qindaqt_session_resident_service_refresh_publisher
    session_resident_service_refresh_publisher.cpp
)
target_include_directories(
    qindaqt_session_resident_service_refresh_publisher PRIVATE "${PROJECT_SOURCE_DIR}"
)
target_link_libraries(
    qindaqt_session_resident_service_refresh_publisher
    PRIVATE QindaQt::SessionSupervisorSupport
)
qindaqt_enable_warnings(qindaqt_session_resident_service_refresh_publisher)
qt_add_executable(qindaqt_resident_service_refresh_tests tst_resident_service_refresh.cpp)
target_include_directories(qindaqt_resident_service_refresh_tests PRIVATE "${PROJECT_SOURCE_DIR}")
target_link_libraries(
    qindaqt_resident_service_refresh_tests
    PRIVATE QindaQt::SessionSupervisorSupport Qt6::Core Qt6::DBus Qt6::Test
)
target_compile_definitions(
    qindaqt_resident_service_refresh_tests
    PRIVATE
        QINDAQT_RESIDENT_SERVICE_REFRESH_PUBLISHER="$<TARGET_FILE:qindaqt_session_resident_service_refresh_publisher>"
)
add_dependencies(
    qindaqt_resident_service_refresh_tests
    qindaqt_session_resident_service_refresh_publisher
)
qindaqt_enable_warnings(qindaqt_resident_service_refresh_tests)
add_test(NAME qindaqt.session-resident-service-refresh
    COMMAND dbus-run-session -- $<TARGET_FILE:qindaqt_resident_service_refresh_tests>)
set_tests_properties(
    qindaqt.session-resident-service-refresh PROPERTIES TIMEOUT 15 LABELS "session;dbus"
)

qt_add_executable(qindaqt_powerdevil_lifetime_tests tst_powerdevil_lifetime.cpp)
target_link_libraries(qindaqt_powerdevil_lifetime_tests PRIVATE
    QindaQt::SessionSupervisorSupport Qt6::Test)
target_compile_definitions(qindaqt_powerdevil_lifetime_tests PRIVATE
    QINDAQT_SESSION_TOKEN_CHILD_HELPER="$<TARGET_FILE:qindaqt_session_token_child_helper>"
    QINDAQT_SESSION_PLAIN_CHILD_HELPER="$<TARGET_FILE:qindaqt_session_plain_child_helper>")
add_dependencies(qindaqt_powerdevil_lifetime_tests
    qindaqt_session_token_child_helper qindaqt_session_plain_child_helper)
qindaqt_enable_warnings(qindaqt_powerdevil_lifetime_tests)
add_test(NAME qindaqt.session-powerdevil-lifetime
    COMMAND dbus-run-session -- $<TARGET_FILE:qindaqt_powerdevil_lifetime_tests>)
set_tests_properties(qindaqt.session-powerdevil-lifetime PROPERTIES
    TIMEOUT 20 LABELS "session;service;dbus")

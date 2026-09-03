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
        TIMEOUT 45
        LABELS "session;service;dbus"
        ENVIRONMENT "DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent"
)

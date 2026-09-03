# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_executable(qindaqt_session_actions_tests tst_session_actions_client.cpp)
target_link_libraries(
    qindaqt_session_actions_tests
    PRIVATE QindaQt::SessionActions Qt6::Core Qt6::DBus Qt6::Test
)
qindaqt_enable_warnings(qindaqt_session_actions_tests)
add_test(
    NAME qindaqt.session-actions-client
    COMMAND dbus-run-session -- $<TARGET_FILE:qindaqt_session_actions_tests>
)
set_tests_properties(
    qindaqt.session-actions-client
    PROPERTIES
        TIMEOUT 25
        LABELS "session;service;dbus"
        ENVIRONMENT "DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent"
)

add_test(
    NAME qindaqt.session-actions-boundary
    COMMAND "${CMAKE_COMMAND}"
        -DSOURCE_ROOT=${PROJECT_SOURCE_DIR}
        -P "${CMAKE_CURRENT_SOURCE_DIR}/check_boundary.cmake"
)
set_tests_properties(
    qindaqt.session-actions-boundary
    PROPERTIES LABELS "session;service;boundary;hostile"
)

add_test(
    NAME qindaqt.session-actions-boundary-poison
    COMMAND "${CMAKE_COMMAND}"
        -DSOURCE_ROOT=${PROJECT_SOURCE_DIR}
        -DPOISON_ROOT=${CMAKE_CURRENT_BINARY_DIR}/boundary-poison
        -DCHECK_SCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/check_boundary.cmake
        -P "${CMAKE_CURRENT_SOURCE_DIR}/check_boundary_negative.cmake"
)
set_tests_properties(
    qindaqt.session-actions-boundary-poison
    PROPERTIES LABELS "session;service;boundary;hostile;poison"
)

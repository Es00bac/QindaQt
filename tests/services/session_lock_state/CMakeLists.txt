# SPDX-License-Identifier: LGPL-3.0-or-later

qt_add_executable(
    qindaqt_session_lock_authentication_tests
    tst_session_lock_authentication.cpp
    support/fake_session_lock_transport.h
)
target_link_libraries(
    qindaqt_session_lock_authentication_tests
    PRIVATE QindaQt::SessionLockState Qt6::Test
)
qindaqt_enable_warnings(qindaqt_session_lock_authentication_tests)
add_test(
    NAME qindaqt.session-lock-authentication
    COMMAND qindaqt_session_lock_authentication_tests
)

qt_add_executable(
    qindaqt_session_lock_transition_tests
    tst_session_lock_transitions.cpp
    support/fake_session_lock_transport.h
)
target_link_libraries(
    qindaqt_session_lock_transition_tests
    PRIVATE QindaQt::SessionLockState Qt6::Test
)
qindaqt_enable_warnings(qindaqt_session_lock_transition_tests)
add_test(
    NAME qindaqt.session-lock-transitions
    COMMAND qindaqt_session_lock_transition_tests
)

qt_add_executable(
    qindaqt_qt_session_lock_transport_tests
    tst_qt_session_lock_transport.cpp
)
target_link_libraries(
    qindaqt_qt_session_lock_transport_tests
    PRIVATE QindaQt::SessionLockState Qt6::Core Qt6::DBus Qt6::Test
)
qindaqt_enable_warnings(qindaqt_qt_session_lock_transport_tests)
add_test(
    NAME qindaqt.session-lock-qt-transport
    COMMAND qindaqt_qt_session_lock_transport_tests
)
set_tests_properties(
    qindaqt.session-lock-qt-transport
    PROPERTIES TIMEOUT 20 LABELS "service;dbus"
)

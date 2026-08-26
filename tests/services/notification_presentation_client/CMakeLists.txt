# SPDX-License-Identifier: LGPL-3.0-or-later

qt_add_executable(
    qindaqt_notification_presentation_client_tests
    tst_notification_presentation_client.cpp
)
target_link_libraries(
    qindaqt_notification_presentation_client_tests
    PRIVATE QindaQt::NotificationPresentationClient Qt6::Test
)
qindaqt_enable_warnings(qindaqt_notification_presentation_client_tests)
add_test(
    NAME qindaqt.notification-presentation-client
    COMMAND qindaqt_notification_presentation_client_tests
)

qt_add_executable(
    qindaqt_notification_presentation_client_operation_tests
    tst_notification_presentation_client_operations.cpp
    support/notification_presentation_client_test_support.h
)
target_link_libraries(
    qindaqt_notification_presentation_client_operation_tests
    PRIVATE QindaQt::NotificationPresentationClient Qt6::Test
)
qindaqt_enable_warnings(
    qindaqt_notification_presentation_client_operation_tests
)
add_test(
    NAME qindaqt.notification-presentation-client-operations
    COMMAND qindaqt_notification_presentation_client_operation_tests
)

qt_add_executable(
    qindaqt_qt_notification_presentation_transport_tests
    tst_qt_notification_presentation_transport.cpp
    ../notification_host/support/notification_host_test_support.h
)
target_link_libraries(
    qindaqt_qt_notification_presentation_transport_tests
    PRIVATE
        QindaQt::NotificationHost
        QindaQt::NotificationPresentationClient
        Qt6::Core
        Qt6::DBus
        Qt6::Test
)
qindaqt_enable_warnings(qindaqt_qt_notification_presentation_transport_tests)
add_test(
    NAME qindaqt.notification-presentation-qt-transport
    COMMAND qindaqt_qt_notification_presentation_transport_tests
)
set_tests_properties(
    qindaqt.notification-presentation-qt-transport
    PROPERTIES TIMEOUT 20 LABELS "service;dbus"
)

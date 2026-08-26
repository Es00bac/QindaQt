# SPDX-License-Identifier: GPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Core DBus Test)

qt_add_executable(qindaqt_notification_deadline_tests
    tst_notification_deadlines.cpp
    support/notification_host_test_support.h
)
target_link_libraries(
    qindaqt_notification_deadline_tests
    PRIVATE QindaQt::NotificationHost Qt6::Core Qt6::DBus Qt6::Test
)
qindaqt_enable_warnings(qindaqt_notification_deadline_tests)
add_test(
    NAME qindaqt.notification-host-deadlines
    COMMAND qindaqt_notification_deadline_tests
)
set_tests_properties(
    qindaqt.notification-host-deadlines
    PROPERTIES TIMEOUT 20 LABELS "service;dbus"
)

qt_add_executable(qindaqt_notification_host_lifecycle_tests
    tst_notification_host_lifecycle.cpp
    support/notification_host_test_support.h
)
target_link_libraries(
    qindaqt_notification_host_lifecycle_tests
    PRIVATE QindaQt::NotificationHost Qt6::Core Qt6::DBus Qt6::Test
)
qindaqt_enable_warnings(qindaqt_notification_host_lifecycle_tests)
add_test(
    NAME qindaqt.notification-host-lifecycle
    COMMAND qindaqt_notification_host_lifecycle_tests
)
set_tests_properties(
    qindaqt.notification-host-lifecycle
    PROPERTIES TIMEOUT 20 LABELS "service;dbus"
)

qt_add_executable(qindaqt_notification_presentation_dbus_tests
    tst_notification_presentation_dbus.cpp
    support/notification_host_test_support.h
)
target_link_libraries(
    qindaqt_notification_presentation_dbus_tests
    PRIVATE
        QindaQt::NotificationHost
        QindaQt::NotificationPresentationProtocol
        Qt6::Core
        Qt6::DBus
        Qt6::Test
)
qindaqt_enable_warnings(qindaqt_notification_presentation_dbus_tests)
add_test(
    NAME qindaqt.notification-host-presentation-dbus
    COMMAND qindaqt_notification_presentation_dbus_tests
)
set_tests_properties(
    qindaqt.notification-host-presentation-dbus
    PROPERTIES TIMEOUT 20 LABELS "service;dbus"
)

qt_add_executable(qindaqt_qt_notification_deadline_scheduler_tests
    tst_qt_deadline_scheduler.cpp
)
target_link_libraries(
    qindaqt_qt_notification_deadline_scheduler_tests
    PRIVATE QindaQt::NotificationHost Qt6::Core Qt6::Test
)
qindaqt_enable_warnings(qindaqt_qt_notification_deadline_scheduler_tests)
add_test(
    NAME qindaqt.notification-host-qt-scheduler
    COMMAND qindaqt_qt_notification_deadline_scheduler_tests
)
set_tests_properties(
    qindaqt.notification-host-qt-scheduler
    PROPERTIES TIMEOUT 20 LABELS "service"
)

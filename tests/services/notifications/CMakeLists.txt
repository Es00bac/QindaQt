# SPDX-License-Identifier: GPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS DBus)

qt_add_executable(qindaqt_notification_submission_tests
    tst_notification_submission.cpp
    support/notification_test_support.h
)
target_link_libraries(qindaqt_notification_submission_tests
    PRIVATE qindaqt_notifications Qt6::Test
)
qindaqt_enable_warnings(qindaqt_notification_submission_tests)
add_test(NAME qindaqt.notifications-submission
         COMMAND qindaqt_notification_submission_tests)

qt_add_executable(qindaqt_notification_interaction_tests
    tst_notification_interaction.cpp
    support/notification_test_support.h
)
target_link_libraries(qindaqt_notification_interaction_tests
    PRIVATE qindaqt_notifications Qt6::Test
)
qindaqt_enable_warnings(qindaqt_notification_interaction_tests)
add_test(NAME qindaqt.notifications-interaction
         COMMAND qindaqt_notification_interaction_tests)

qt_add_executable(qindaqt_notification_dbus_tests
    tst_freedesktop_notification_dbus.cpp
    support/notification_test_support.h
)
target_link_libraries(qindaqt_notification_dbus_tests
    PRIVATE qindaqt_notification_dbus Qt6::DBus Qt6::Test
)
qindaqt_enable_warnings(qindaqt_notification_dbus_tests)
add_test(NAME qindaqt.notifications-dbus COMMAND qindaqt_notification_dbus_tests)

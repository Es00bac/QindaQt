# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_executable(
    qindaqt_notification_presentation_policy_tests
    tst_notification_interruption_policy.cpp
)
target_link_libraries(
    qindaqt_notification_presentation_policy_tests
    PRIVATE QindaQt::NotificationPresentationPolicy Qt6::Test
)
qindaqt_enable_warnings(qindaqt_notification_presentation_policy_tests)
add_test(
    NAME qindaqt.notification-presentation-policy
    COMMAND qindaqt_notification_presentation_policy_tests
)

qt_add_executable(
    qindaqt_notification_privacy_policy_tests
    tst_notification_privacy_policy.cpp
)
target_link_libraries(
    qindaqt_notification_privacy_policy_tests
    PRIVATE QindaQt::NotificationPresentationPolicy Qt6::Test
)
qindaqt_enable_warnings(qindaqt_notification_privacy_policy_tests)
add_test(
    NAME qindaqt.notification-privacy-policy
    COMMAND qindaqt_notification_privacy_policy_tests
)

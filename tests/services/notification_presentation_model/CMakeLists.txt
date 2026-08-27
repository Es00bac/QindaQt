# SPDX-License-Identifier: LGPL-3.0-or-later

qt_add_executable(
    qindaqt_notification_presentation_model_tests
    tst_notification_presentation_model.cpp
)
target_link_libraries(
    qindaqt_notification_presentation_model_tests
    PRIVATE QindaQt::NotificationPresentationModel Qt6::Test
)
qindaqt_enable_warnings(qindaqt_notification_presentation_model_tests)
add_test(
    NAME qindaqt.notification-presentation-model
    COMMAND qindaqt_notification_presentation_model_tests
)

qt_add_executable(
    qindaqt_notification_presentation_dnd_tests
    tst_notification_presentation_dnd.cpp
)
target_link_libraries(
    qindaqt_notification_presentation_dnd_tests
    PRIVATE QindaQt::NotificationPresentationModel Qt6::Test
)
qindaqt_enable_warnings(qindaqt_notification_presentation_dnd_tests)
add_test(
    NAME qindaqt.notification-presentation-dnd
    COMMAND qindaqt_notification_presentation_dnd_tests
)

qt_add_executable(
    qindaqt_notification_presentation_privacy_tests
    tst_notification_presentation_privacy.cpp
)
target_link_libraries(
    qindaqt_notification_presentation_privacy_tests
    PRIVATE QindaQt::NotificationPresentationModel Qt6::Test
)
qindaqt_enable_warnings(qindaqt_notification_presentation_privacy_tests)
add_test(
    NAME qindaqt.notification-presentation-privacy
    COMMAND qindaqt_notification_presentation_privacy_tests
)

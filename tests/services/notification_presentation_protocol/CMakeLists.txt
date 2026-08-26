# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_executable(
    qindaqt_notification_presentation_protocol_tests
    tst_presentation_protocol.cpp
)
target_link_libraries(
    qindaqt_notification_presentation_protocol_tests
    PRIVATE QindaQt::NotificationPresentationProtocol Qt6::Test
)
qindaqt_enable_warnings(qindaqt_notification_presentation_protocol_tests)
add_test(
    NAME qindaqt.notification-presentation-protocol
    COMMAND qindaqt_notification_presentation_protocol_tests
)

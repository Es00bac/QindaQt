# SPDX-License-Identifier: LGPL-3.0-or-later

qt_add_library(qindaqt_notification_presentation_policy STATIC)
add_library(
    QindaQt::NotificationPresentationPolicy
    ALIAS qindaqt_notification_presentation_policy
)

target_sources(
    qindaqt_notification_presentation_policy
    PRIVATE
        src/notification_interruption_policy.cpp
        src/notification_privacy_policy.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/services/notification_presentation_policy/notification_interruption_policy.h
            include/qindaqt/services/notification_presentation_policy/notification_privacy_policy.h
)
target_link_libraries(
    qindaqt_notification_presentation_policy
    PUBLIC QindaQt::NotificationPresentationProtocol Qt6::Core
)
target_compile_features(qindaqt_notification_presentation_policy PUBLIC cxx_std_20)
set_target_properties(
    qindaqt_notification_presentation_policy PROPERTIES CXX_EXTENSIONS OFF
)
qindaqt_enable_warnings(qindaqt_notification_presentation_policy)

install(
    TARGETS qindaqt_notification_presentation_policy
    EXPORT QindaQtTargets
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

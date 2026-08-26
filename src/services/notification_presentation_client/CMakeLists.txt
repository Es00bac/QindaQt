# SPDX-License-Identifier: LGPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS DBus)

qt_add_library(qindaqt_notification_presentation_client STATIC)
add_library(
    QindaQt::NotificationPresentationClient
    ALIAS qindaqt_notification_presentation_client
)

target_sources(
    qindaqt_notification_presentation_client
    PRIVATE
        src/notification_presentation_client.cpp
        src/operation_lifecycle.cpp
        src/operation_validation.cpp
        src/operation_validation.h
        src/qt_notification_presentation_transport.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/services/notification_presentation_client/notification_presentation_client.h
            include/qindaqt/services/notification_presentation_client/presentation_transport.h
            include/qindaqt/services/notification_presentation_client/qt_notification_presentation_transport.h
)
target_link_libraries(
    qindaqt_notification_presentation_client
    PUBLIC QindaQt::NotificationPresentationProtocol Qt6::Core Qt6::DBus
)
target_compile_features(qindaqt_notification_presentation_client PUBLIC cxx_std_20)
set_target_properties(
    qindaqt_notification_presentation_client PROPERTIES CXX_EXTENSIONS OFF
)
qindaqt_enable_warnings(qindaqt_notification_presentation_client)

install(
    TARGETS qindaqt_notification_presentation_client
    EXPORT QindaQtTargets
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

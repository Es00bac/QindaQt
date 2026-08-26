# SPDX-License-Identifier: LGPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS DBus)

qt_add_library(qindaqt_notifications STATIC)
add_library(QindaQt::Notifications ALIAS qindaqt_notifications)

target_sources(
    qindaqt_notifications
    PRIVATE
        src/notification_capacity.cpp
        src/notification_capacity_p.h
        src/notification_clock.cpp
        src/notification_id_allocator.cpp
        src/notification_id_allocator_p.h
        src/notification_service.cpp
        src/notification_types.cpp
        src/notification_validation.cpp
        src/notification_validation_p.h
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/services/notifications/notification_backend.h
            include/qindaqt/services/notifications/notification_clock.h
            include/qindaqt/services/notifications/notification_limits.h
            include/qindaqt/services/notifications/notification_service.h
            include/qindaqt/services/notifications/notification_types.h
)

target_link_libraries(qindaqt_notifications PUBLIC Qt6::Core)
target_compile_features(qindaqt_notifications PUBLIC cxx_std_20)
qindaqt_enable_warnings(qindaqt_notifications)

install(
    TARGETS qindaqt_notifications
    EXPORT QindaQtTargets
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

qt_add_library(qindaqt_notification_dbus STATIC)
add_library(QindaQt::NotificationDBus ALIAS qindaqt_notification_dbus)

target_sources(
    qindaqt_notification_dbus
    PRIVATE
        src/freedesktop_notification_codec.cpp
        src/freedesktop_notification_codec_p.h
        src/freedesktop_notification_object.cpp
        src/freedesktop_notification_object_p.h
        src/freedesktop_notification_server.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/services/notifications/freedesktop_notification_server.h
)

target_link_libraries(
    qindaqt_notification_dbus
    PUBLIC
        qindaqt_notifications
        Qt6::DBus
)
target_compile_features(qindaqt_notification_dbus PUBLIC cxx_std_20)
qindaqt_enable_warnings(qindaqt_notification_dbus)

install(
    TARGETS qindaqt_notification_dbus
    EXPORT QindaQtTargets
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

# SPDX-License-Identifier: GPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Core DBus)

qt_add_library(qindaqt_link_notifications STATIC)
add_library(QindaQt::LinkNotifications ALIAS qindaqt_link_notifications)

target_sources(
    qindaqt_link_notifications
    PRIVATE
        src/app_settings.cpp
        src/fdo_phone_presenter.cpp
        src/icon_cache.cpp
        src/notification_feature.cpp
        src/notifications_adaptor.cpp
        src/notifications_module.cpp
        src/phone_notification.cpp
        src/process_reply_launcher.cpp
        src/recording_presenter.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/services/link_notifications/app_settings.h
            include/qindaqt/services/link_notifications/fdo_phone_presenter.h
            include/qindaqt/services/link_notifications/icon_cache.h
            include/qindaqt/services/link_notifications/notification_feature.h
            include/qindaqt/services/link_notifications/notification_presenter.h
            include/qindaqt/services/link_notifications/notifications_adaptor.h
            include/qindaqt/services/link_notifications/notifications_module.h
            include/qindaqt/services/link_notifications/phone_notification.h
            include/qindaqt/services/link_notifications/process_reply_launcher.h
            include/qindaqt/services/link_notifications/recording_presenter.h
)
target_link_libraries(
    qindaqt_link_notifications
    PUBLIC QindaQt::LinkTransport QindaQt::LinkProtocol Qt6::Core Qt6::DBus
)
target_compile_features(qindaqt_link_notifications PUBLIC cxx_std_20)
set_target_properties(qindaqt_link_notifications PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_link_notifications)

install(
    TARGETS qindaqt_link_notifications
    EXPORT QindaQtTargets
    ARCHIVE COMPONENT QindaQtLink
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    COMPONENT QindaQtLink
)

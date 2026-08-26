# SPDX-License-Identifier: LGPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Core DBus)

qt_add_library(qindaqt_notification_host STATIC)
add_library(QindaQt::NotificationHost ALIAS qindaqt_notification_host)

target_sources(
    qindaqt_notification_host
    PRIVATE
        src/dbus_service_name_validation.cpp
        src/dbus_service_name_validation_p.h
        src/qt_deadline_scheduler.cpp
        src/resident_notification_host.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/services/notification_host/deadline_scheduler.h
            include/qindaqt/services/notification_host/qt_deadline_scheduler.h
            include/qindaqt/services/notification_host/resident_notification_host.h
)

target_link_libraries(
    qindaqt_notification_host
    PUBLIC
        QindaQt::NotificationDBus
        Qt6::Core
        Qt6::DBus
)
target_compile_features(qindaqt_notification_host PUBLIC cxx_std_20)
set_target_properties(qindaqt_notification_host PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_notification_host)

install(
    TARGETS qindaqt_notification_host
    EXPORT QindaQtTargets
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

qt_add_executable(qindaqt-notification-host app/main.cpp)
target_link_libraries(
    qindaqt-notification-host
    PRIVATE QindaQt::NotificationHost Qt6::Core Qt6::DBus
)
target_compile_definitions(
    qindaqt-notification-host PRIVATE QINDAQT_VERSION="${PROJECT_VERSION}"
)
set_target_properties(qindaqt-notification-host PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt-notification-host)

install(
    TARGETS qindaqt-notification-host
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)

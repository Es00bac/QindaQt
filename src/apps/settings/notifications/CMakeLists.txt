# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_library(qindaqt_settings_notifications STATIC)
target_sources(
    qindaqt_settings_notifications
    PRIVATE
        notification_schedule_model.cpp
        notification_application_settings_model.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}"
        FILES
            notification_schedule_model.h
            qindaqt/apps/settings_notifications/notification_application_settings_model.h
)
target_link_libraries(
    qindaqt_settings_notifications
    PUBLIC QindaQt::SettingsClient QindaQt::NotificationPresentationPolicy
           QindaQt::ApplicationCatalog Qt6::Core
)
target_compile_features(qindaqt_settings_notifications PUBLIC cxx_std_20)
set_target_properties(
    qindaqt_settings_notifications
    PROPERTIES CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON
)
qindaqt_enable_warnings(qindaqt_settings_notifications)

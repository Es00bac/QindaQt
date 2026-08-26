# SPDX-License-Identifier: LGPL-3.0-or-later

qt_add_library(qindaqt_notification_presentation_model STATIC)
add_library(
    QindaQt::NotificationPresentationModel
    ALIAS qindaqt_notification_presentation_model
)
target_sources(
    qindaqt_notification_presentation_model
    PRIVATE
        src/notification_list_model.cpp
        src/notification_presentation_controller.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/services/notification_presentation_model/notification_list_model.h
            include/qindaqt/services/notification_presentation_model/notification_presentation_controller.h
)
target_link_libraries(
    qindaqt_notification_presentation_model
    PUBLIC QindaQt::NotificationPresentationClient Qt6::Core
)
target_compile_features(qindaqt_notification_presentation_model PUBLIC cxx_std_20)
set_target_properties(
    qindaqt_notification_presentation_model PROPERTIES CXX_EXTENSIONS OFF
)
qindaqt_enable_warnings(qindaqt_notification_presentation_model)
install(
    TARGETS qindaqt_notification_presentation_model
    EXPORT QindaQtTargets
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

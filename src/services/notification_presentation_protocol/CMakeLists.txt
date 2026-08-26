# SPDX-License-Identifier: LGPL-3.0-or-later

qt_add_library(qindaqt_notification_presentation_protocol STATIC)
add_library(
    QindaQt::NotificationPresentationProtocol
    ALIAS qindaqt_notification_presentation_protocol
)

target_sources(
    qindaqt_notification_presentation_protocol
    PRIVATE
        src/presentation_access_token.cpp
        src/presentation_snapshot.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/services/notification_presentation/presentation_access_token.h
            include/qindaqt/services/notification_presentation/presentation_snapshot.h
            include/qindaqt/services/notification_presentation/wire_contract.h
)
target_link_libraries(
    qindaqt_notification_presentation_protocol PUBLIC Qt6::Core Qt6::DBus
)
target_compile_features(qindaqt_notification_presentation_protocol PUBLIC cxx_std_20)
set_target_properties(
    qindaqt_notification_presentation_protocol PROPERTIES CXX_EXTENSIONS OFF
)
qindaqt_enable_warnings(qindaqt_notification_presentation_protocol)

install(
    TARGETS qindaqt_notification_presentation_protocol
    EXPORT QindaQtTargets
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

# SPDX-License-Identifier: GPL-3.0-or-later
# Independent wire policy, request lifetime, selected-session attachment and
# platform adapters. The interface aggregate carries no central implementation.
function(portal_library target alias)
    qt_add_library(${target} STATIC)
    add_library(QindaQt::${alias} ALIAS ${target})
    target_include_directories(${target} PUBLIC ../include)
    target_compile_features(${target} PUBLIC cxx_std_20)
    target_compile_definitions(${target} PRIVATE QT_NO_KEYWORDS)
    set_target_properties(${target} PROPERTIES CXX_EXTENSIONS OFF)
    qindaqt_enable_warnings(${target})
    install(TARGETS ${target} EXPORT QindaQtTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT QindaQtPortalP0
        FILE_SET HEADERS DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}" COMPONENT QindaQtPortalP0)
endfunction()
portal_library(qindaqt_portal_requests PortalRequests)
target_sources(qindaqt_portal_requests PRIVATE ../src/request_registry.cpp
    PUBLIC FILE_SET HEADERS BASE_DIRS ../include FILES ../include/qindaqt/services/portal/request_registry.h)
target_link_libraries(qindaqt_portal_requests PUBLIC Qt6::Core Qt6::DBus)
portal_library(qindaqt_portal_session PortalSession)
target_sources(qindaqt_portal_session PRIVATE ../src/session_binding.cpp
    PUBLIC FILE_SET HEADERS BASE_DIRS ../include FILES ../include/qindaqt/services/portal/session_binding.h)
target_link_libraries(qindaqt_portal_session PUBLIC Qt6::Core Qt6::DBus QindaQt::CompositorAttachment)
portal_library(qindaqt_portal_access_policy PortalAccessPolicy)
target_sources(qindaqt_portal_access_policy PRIVATE ../src/access_policy.cpp
    PUBLIC FILE_SET HEADERS BASE_DIRS ../include FILES ../include/qindaqt/services/portal/access_consent.h)
target_link_libraries(qindaqt_portal_access_policy PUBLIC QindaQt::PortalRequests)
portal_library(qindaqt_portal_access PortalAccess)
target_sources(qindaqt_portal_access PRIVATE ../src/access_adaptor.cpp ../src/process_consent.cpp
    PUBLIC FILE_SET HEADERS BASE_DIRS ../include FILES
    ../include/qindaqt/services/portal/access_adaptor.h ../include/qindaqt/services/portal/process_consent.h)
target_link_libraries(qindaqt_portal_access PUBLIC QindaQt::PortalAccessPolicy QindaQt::PortalSession QindaQt::SessionLockState)
portal_library(qindaqt_portal_inhibit PortalInhibit)
target_sources(qindaqt_portal_inhibit PRIVATE ../src/idle_inhibition.cpp ../src/inhibit_adaptor.cpp
    PUBLIC FILE_SET HEADERS BASE_DIRS ../include FILES
    ../include/qindaqt/services/portal/idle_inhibition.h ../include/qindaqt/services/portal/inhibit_adaptor.h)
target_link_libraries(qindaqt_portal_inhibit PUBLIC QindaQt::PortalRequests QindaQt::PowerClient)
add_library(qindaqt_portal_foundation INTERFACE)
add_library(QindaQt::PortalFoundation ALIAS qindaqt_portal_foundation)
target_link_libraries(qindaqt_portal_foundation INTERFACE QindaQt::PortalAccess QindaQt::PortalInhibit)
install(TARGETS qindaqt_portal_foundation EXPORT QindaQtTargets COMPONENT QindaQtPortalP0)
portal_library(qindaqt_portal_notification_policy PortalNotificationPolicy)
target_sources(qindaqt_portal_notification_policy PRIVATE ../src/notification_policy.cpp
    PUBLIC FILE_SET HEADERS BASE_DIRS ../include FILES ../include/qindaqt/services/portal/notification_policy.h)
target_link_libraries(qindaqt_portal_notification_policy PUBLIC Qt6::Core Qt6::DBus)
portal_library(qindaqt_portal_notifications PortalNotifications)
target_sources(qindaqt_portal_notifications PRIVATE ../src/notification_icon.cpp
    ../src/native_notifications.cpp ../src/notification_adaptor.cpp
    PUBLIC FILE_SET HEADERS BASE_DIRS ../include FILES
    ../include/qindaqt/services/portal/native_notifications.h ../include/qindaqt/services/portal/notification_adaptor.h)
target_link_libraries(qindaqt_portal_notifications PUBLIC QindaQt::PortalNotificationPolicy QindaQt::PortalRequests PRIVATE Qt6::Gui)

portal_library(qindaqt_portal_email_policy PortalEmailPolicy)
target_sources(qindaqt_portal_email_policy PRIVATE ../src/email_policy.cpp
    PUBLIC FILE_SET HEADERS BASE_DIRS ../include FILES ../include/qindaqt/services/portal/email_policy.h)
target_link_libraries(qindaqt_portal_email_policy PUBLIC Qt6::Core Qt6::DBus)
portal_library(qindaqt_portal_email PortalEmail)
target_sources(qindaqt_portal_email PRIVATE ../src/email_adaptor.cpp
    PUBLIC FILE_SET HEADERS BASE_DIRS ../include FILES ../include/qindaqt/services/portal/email_adaptor.h)
target_link_libraries(qindaqt_portal_email PUBLIC QindaQt::PortalRequests QindaQt::PortalEmailPolicy QindaQt::ApplicationUriOpener)

portal_library(qindaqt_portal_composition PortalComposition)
target_sources(qindaqt_portal_composition PRIVATE ../src/foundation_composition.cpp
    PUBLIC FILE_SET HEADERS BASE_DIRS ../include FILES ../include/qindaqt/services/portal/foundation_composition.h)
target_link_libraries(qindaqt_portal_composition PUBLIC QindaQt::PortalFoundation QindaQt::PortalNotifications QindaQt::PortalEmail QindaQt::PortalChoosers QindaQt::PortalCapture QindaQt::PortalShortcuts QindaQt::PortalMisc)

portal_library(qindaqt_portal_chooser_policy PortalChooserPolicy)
target_sources(qindaqt_portal_chooser_policy PRIVATE ../src/chooser_wire.cpp ../src/chooser_frames.cpp
    ../src/file_chooser_policy.cpp ../src/app_chooser_policy.cpp
    PUBLIC FILE_SET HEADERS BASE_DIRS ../include FILES ../include/qindaqt/services/portal/chooser_types.h)
target_link_libraries(qindaqt_portal_chooser_policy PUBLIC QindaQt::PortalAccessPolicy QindaQt::ApplicationCatalog)
portal_library(qindaqt_portal_choosers PortalChoosers)
target_sources(qindaqt_portal_choosers PRIVATE ../src/file_chooser_adaptor.cpp ../src/app_chooser_adaptor.cpp ../src/process_chooser.cpp
    PUBLIC FILE_SET HEADERS BASE_DIRS ../include FILES ../include/qindaqt/services/portal/chooser_ui.h
    ../include/qindaqt/services/portal/file_chooser_adaptor.h ../include/qindaqt/services/portal/app_chooser_adaptor.h
    ../include/qindaqt/services/portal/process_chooser.h)
target_link_libraries(qindaqt_portal_choosers PUBLIC QindaQt::PortalChooserPolicy QindaQt::PortalSession)

portal_library(qindaqt_portal_capture_policy PortalCapturePolicy)
target_sources(qindaqt_portal_capture_policy PRIVATE ../src/capture_policy.cpp ../src/capture_wire.cpp
    PUBLIC FILE_SET HEADERS BASE_DIRS ../include FILES ../include/qindaqt/services/portal/capture_types.h)
target_link_libraries(qindaqt_portal_capture_policy PUBLIC QindaQt::PortalAccessPolicy)
portal_library(qindaqt_portal_capture PortalCapture)
target_sources(qindaqt_portal_capture PRIVATE ../src/capture_sessions.cpp ../src/capture_sessions_p.h
    ../src/screenshot_adaptor.cpp ../src/screencast_adaptor.cpp ../src/process_capture.cpp
    PUBLIC FILE_SET HEADERS BASE_DIRS ../include FILES ../include/qindaqt/services/portal/capture_ui.h
    ../include/qindaqt/services/portal/process_capture.h ../include/qindaqt/services/portal/screenshot_adaptor.h
    ../include/qindaqt/services/portal/screencast_adaptor.h)
target_link_libraries(qindaqt_portal_capture PUBLIC QindaQt::PortalCapturePolicy QindaQt::PortalSession)

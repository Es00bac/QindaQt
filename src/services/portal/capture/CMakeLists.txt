# SPDX-License-Identifier: GPL-3.0-or-later
set(capture_helper_sources main.cpp native_capture_admission.cpp native_capture_admission.h
    capture_dialog.cpp capture_dialog.h color_picker.cpp color_picker.h)
qt_add_executable(qindaqt-portal-capture ${capture_helper_sources})
target_link_libraries(qindaqt-portal-capture PRIVATE QindaQt::PortalCapturePolicy
    QindaQt::CompositorCapture QindaQt::WaylandScreenCast QindaQt::SessionLockState
    QindaQt::ForeignParent Qt6::Widgets)
qindaqt_enable_warnings(qindaqt-portal-capture)
install(TARGETS qindaqt-portal-capture RUNTIME DESTINATION "${KDE_INSTALL_LIBEXECDIR}" COMPONENT QindaQtPortalP0)
configure_file(org.qindaqt.PortalCapture.desktop.in "${CMAKE_CURRENT_BINARY_DIR}/org.qindaqt.PortalCapture.desktop" @ONLY)
configure_file(org.qindaqt.PortalBackend.desktop.in "${CMAKE_CURRENT_BINARY_DIR}/org.qindaqt.PortalBackend.desktop" @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/org.qindaqt.PortalCapture.desktop" "${CMAKE_CURRENT_BINARY_DIR}/org.qindaqt.PortalBackend.desktop"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/applications" COMPONENT QindaQtPortalP0)

# AGENT-CONTRACT: protected capture consumes the qualified fork's INSTALLED
# public protocol only. The old fork690 stage has no authority header and
# cannot qualify this backend; no source-tree header or path fallback exists.
find_path(QINDAQT_CAPTURE_AUTHORITY_INCLUDE_DIR
    NAMES qindaqt-kwin/capture-authority/protocol.h)
if(QINDAQT_CAPTURE_AUTHORITY_INCLUDE_DIR)
    qt_add_library(qindaqt_portal_capture_authority STATIC
        authority/packet.cpp authority/packet.h authority/channel.cpp authority/channel.h)
    target_include_directories(qindaqt_portal_capture_authority PUBLIC
        "${QINDAQT_CAPTURE_AUTHORITY_INCLUDE_DIR}")
    target_link_libraries(qindaqt_portal_capture_authority PUBLIC Qt6::Core Qt6::DBus
        PRIVATE QindaQt::CompositorNames)
    target_compile_features(qindaqt_portal_capture_authority PUBLIC cxx_std_20)
    target_compile_definitions(qindaqt_portal_capture_authority PRIVATE QT_NO_KEYWORDS)
    qindaqt_enable_warnings(qindaqt_portal_capture_authority)

    qt_add_library(qindaqt_portal_protected_capture STATIC
        backend/authority_capture.cpp backend/authority_capture.h
        native_capture_admission.cpp native_capture_admission.h)
    target_link_libraries(qindaqt_portal_protected_capture PUBLIC QindaQt::PortalCapture
        QindaQt::SessionLockState qindaqt_portal_capture_authority PRIVATE QindaQt::CompositorNames)
    target_compile_features(qindaqt_portal_protected_capture PUBLIC cxx_std_20)
    target_compile_definitions(qindaqt_portal_protected_capture PRIVATE QT_NO_KEYWORDS)
    qindaqt_enable_warnings(qindaqt_portal_protected_capture)

    qt_add_executable(qindaqt-portal-capture-backend backend/main.cpp)
    target_link_libraries(qindaqt-portal-capture-backend PRIVATE qindaqt_portal_protected_capture)
    qindaqt_enable_warnings(qindaqt-portal-capture-backend)
    install(TARGETS qindaqt-portal-capture-backend
        RUNTIME DESTINATION "${KDE_INSTALL_LIBEXECDIR}" COMPONENT QindaQtPortalP0)
    configure_file(org.qindaqt.PortalCaptureBackend.desktop.in
        "${CMAKE_CURRENT_BINARY_DIR}/org.qindaqt.PortalCaptureBackend.desktop" @ONLY)
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/org.qindaqt.PortalCaptureBackend.desktop"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/applications" COMPONENT QindaQtPortalP0)
else()
    message(STATUS "Protected capture backend unavailable: qualified installed authority header absent")
endif()

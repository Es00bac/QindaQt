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

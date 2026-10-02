# SPDX-License-Identifier: GPL-3.0-or-later
# AGENT-CONTRACT: protected capture consumes the qualified fork's INSTALLED
# public protocol only. The old fork690 stage has no authority header and
# cannot qualify this backend; no source-tree header or path fallback exists.
find_path(QINDAQT_CAPTURE_AUTHORITY_INCLUDE_DIR
    NAMES qindaqt-kwin/capture-authority/protocol.h)
if(QINDAQT_CAPTURE_AUTHORITY_INCLUDE_DIR)
    set(authority_paths "${QINDAQT_CAPTURE_AUTHORITY_INCLUDE_DIR}/qindaqt-kwin/capture-authority/launchpaths.h")
    if(NOT EXISTS "${authority_paths}")
        message(FATAL_ERROR "Selected fork authority header lacks frozen configured launchpaths.h")
    endif()
    file(READ "${authority_paths}" authority_path_source)
    set(expected_BrokerExecutable "${KDE_INSTALL_FULL_LIBEXECDIR}/qindaqt-portal-capture-backend")
    set(expected_HelperExecutable "${KDE_INSTALL_FULL_LIBEXECDIR}/qindaqt-portal-capture")
    set(expected_BrokerDesktop "${KDE_INSTALL_FULL_DATADIR}/applications/org.qindaqt.PortalCaptureBackend.desktop")
    set(expected_HelperDesktop "${KDE_INSTALL_FULL_DATADIR}/applications/org.qindaqt.PortalCapture.desktop")
    foreach(name BrokerExecutable HelperExecutable BrokerDesktop HelperDesktop)
        string(REGEX MATCH "${name}\\[\\] = \"([^\"]+)\"" constant "${authority_path_source}")
        if(NOT constant OR NOT "${CMAKE_MATCH_1}" STREQUAL "${expected_${name}}")
            message(FATAL_ERROR "Selected fork/portal fixed capture install path mismatch: ${name}")
        endif()
    endforeach()
    qt_add_library(qindaqt_portal_capture_authority STATIC
        authority/packet.cpp authority/packet.h authority/channel.cpp authority/channel.h)
    target_include_directories(qindaqt_portal_capture_authority PUBLIC
        "${QINDAQT_CAPTURE_AUTHORITY_INCLUDE_DIR}")
    target_link_libraries(qindaqt_portal_capture_authority PUBLIC Qt6::Core Qt6::DBus
        PRIVATE QindaQt::CompositorNames)
    target_compile_features(qindaqt_portal_capture_authority PUBLIC cxx_std_20)
    target_compile_definitions(qindaqt_portal_capture_authority PRIVATE QT_NO_KEYWORDS)
    qindaqt_enable_warnings(qindaqt_portal_capture_authority)

    set(capture_helper_sources main.cpp helper_runtime.cpp helper_runtime.h native_capture_admission.cpp native_capture_admission.h
        capture_dialog.cpp capture_dialog.h color_picker.cpp color_picker.h)
    qt_add_executable(qindaqt-portal-capture ${capture_helper_sources})
    target_link_libraries(qindaqt-portal-capture PRIVATE QindaQt::PortalCapturePolicy
        QindaQt::CompositorCapture QindaQt::WaylandScreenCast QindaQt::SessionLockState
        QindaQt::ForeignParent Qt6::Widgets qindaqt_portal_capture_authority)
    qindaqt_enable_warnings(qindaqt-portal-capture)
    install(TARGETS qindaqt-portal-capture RUNTIME DESTINATION "${KDE_INSTALL_LIBEXECDIR}" COMPONENT QindaQtPortalP0)
    configure_file(org.qindaqt.PortalCapture.desktop.in "${CMAKE_CURRENT_BINARY_DIR}/org.qindaqt.PortalCapture.desktop" @ONLY)
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/org.qindaqt.PortalCapture.desktop"
        DESTINATION "${KDE_INSTALL_DATADIR}/applications" COMPONENT QindaQtPortalP0)

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
        DESTINATION "${KDE_INSTALL_DATADIR}/applications" COMPONENT QindaQtPortalP0)
    foreach(entry Helper Broker)
        if(entry STREQUAL "Helper")
            set(desktop_name PortalCapture)
        else()
            set(desktop_name PortalCaptureBackend)
        endif()
        file(STRINGS "${CMAKE_CURRENT_BINARY_DIR}/org.qindaqt.${desktop_name}.desktop" exec_line REGEX "^Exec=")
        if(NOT exec_line STREQUAL "Exec=${expected_${entry}Executable}")
            message(FATAL_ERROR "Generated capture desktop Exec differs from selected fork ${entry} image")
        endif()
    endforeach()
else()
    message(STATUS "Protected capture backend unavailable: qualified installed authority header absent")
endif()

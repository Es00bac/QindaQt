# SPDX-License-Identifier: GPL-3.0-or-later
find_package(Qt6 6.11 REQUIRED COMPONENTS WaylandClient WaylandClientPrivate GuiPrivate)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LockWayland REQUIRED IMPORTED_TARGET wayland-client)
find_program(QINDAQT_WAYLAND_SCANNER wayland-scanner REQUIRED)
find_file(QINDAQT_SESSION_LOCK_XML ext-session-lock-v1.xml
    PATHS /usr/share/wayland-protocols/staging/ext-session-lock REQUIRED)
enable_language(C)
add_custom_command(OUTPUT ext-session-lock-v1-client-protocol.h
    COMMAND "${QINDAQT_WAYLAND_SCANNER}" client-header "${QINDAQT_SESSION_LOCK_XML}" "${CMAKE_CURRENT_BINARY_DIR}/ext-session-lock-v1-client-protocol.h"
    DEPENDS "${QINDAQT_SESSION_LOCK_XML}")
add_custom_command(OUTPUT ext-session-lock-v1-protocol.c
    COMMAND "${QINDAQT_WAYLAND_SCANNER}" private-code "${QINDAQT_SESSION_LOCK_XML}" "${CMAKE_CURRENT_BINARY_DIR}/ext-session-lock-v1-protocol.c"
    DEPENDS "${QINDAQT_SESSION_LOCK_XML}")
add_library(qindaqt_lock_protocol SHARED protocol_client.cpp protocol_client.h
    "${CMAKE_CURRENT_BINARY_DIR}/ext-session-lock-v1-client-protocol.h"
    "${CMAKE_CURRENT_BINARY_DIR}/ext-session-lock-v1-protocol.c")
target_include_directories(qindaqt_lock_protocol PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}" PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(qindaqt_lock_protocol PUBLIC Qt6::Core PRIVATE PkgConfig::LockWayland)
set_target_properties(qindaqt_lock_protocol PROPERTIES VERSION 1.0 SOVERSION 1)
qindaqt_enable_warnings(qindaqt_lock_protocol)
add_library(qindaqt_session_lock_integration MODULE shell_integration.cpp session-lock.json)
target_include_directories(qindaqt_session_lock_integration PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(qindaqt_session_lock_integration PRIVATE qindaqt_lock_protocol Qt6::WaylandClientPrivate Qt6::GuiPrivate PkgConfig::LockWayland)
qindaqt_enable_warnings(qindaqt_session_lock_integration)
set_target_properties(qindaqt_session_lock_integration PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/plugins/wayland-shell-integration")
install(TARGETS qindaqt_lock_protocol LIBRARY DESTINATION "${KDE_INSTALL_LIBDIR}" COMPONENT Shell)
install(TARGETS qindaqt_session_lock_integration LIBRARY DESTINATION "${KDE_INSTALL_QTPLUGINDIR}/wayland-shell-integration" COMPONENT Shell)

if(BUILD_TESTING)
    qt_add_executable(qindaqt-private-lock-window tests/lock_window_probe.cpp)
    target_link_libraries(qindaqt-private-lock-window PRIVATE qindaqt_lock_protocol Qt6::GuiPrivate)
    target_compile_definitions(qindaqt-private-lock-window PRIVATE QINDAQT_PRIVATE_LOCK_PLUGIN_ROOT="${CMAKE_CURRENT_BINARY_DIR}/plugins")
    add_dependencies(qindaqt-private-lock-window qindaqt_session_lock_integration)
    qindaqt_enable_warnings(qindaqt-private-lock-window)
endif()

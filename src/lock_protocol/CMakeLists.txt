# SPDX-License-Identifier: GPL-3.0-or-later
find_package(Qt6 6.11 REQUIRED COMPONENTS WaylandClient WaylandClientPrivate GuiPrivate)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LockWayland REQUIRED IMPORTED_TARGET wayland-client)
pkg_check_modules(LockXKB REQUIRED IMPORTED_TARGET xkbcommon)
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
add_library(qindaqt_lock_protocol SHARED protocol_client.cpp protocol_client.h keyboard_layout.cpp keyboard_layout.h
    "${CMAKE_CURRENT_BINARY_DIR}/ext-session-lock-v1-client-protocol.h"
    "${CMAKE_CURRENT_BINARY_DIR}/ext-session-lock-v1-protocol.c")
target_include_directories(qindaqt_lock_protocol PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}" PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(qindaqt_lock_protocol PUBLIC Qt6::Core PRIVATE PkgConfig::LockWayland PkgConfig::LockXKB)
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
    find_package(Qt6 REQUIRED COMPONENTS Test)
    find_package(Threads REQUIRED)
    qt_add_executable(tst_lock_protocol_configure tests/tst_configure_policy.cpp)
    target_include_directories(tst_lock_protocol_configure PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(tst_lock_protocol_configure PRIVATE Qt6::Core Qt6::Test)
    qindaqt_enable_warnings(tst_lock_protocol_configure)
    add_test(NAME lock.protocol-configure COMMAND tst_lock_protocol_configure)
    set_tests_properties(lock.protocol-configure PROPERTIES ENVIRONMENT "QT_FATAL_WARNINGS=1" TIMEOUT 30 LABELS "lock;protocol")
    # The real Qt Wayland client and this integration plugin against a fake
    # ext-session-lock compositor (libwayland-server). Skipped (exit 77) when the
    # wayland platform plugin is unavailable.
    pkg_check_modules(LockWaylandServer IMPORTED_TARGET wayland-server)
    if(LockWaylandServer_FOUND)
        add_custom_command(OUTPUT ext-session-lock-v1-server-protocol.h
            COMMAND "${QINDAQT_WAYLAND_SCANNER}" server-header "${QINDAQT_SESSION_LOCK_XML}" "${CMAKE_CURRENT_BINARY_DIR}/ext-session-lock-v1-server-protocol.h"
            DEPENDS "${QINDAQT_SESSION_LOCK_XML}")
        qt_add_executable(tst_lock_protocol_integration tests/tst_session_lock_integration.cpp
            tests/fake_lock_compositor.cpp tests/fake_lock_compositor.h
            "${CMAKE_CURRENT_BINARY_DIR}/ext-session-lock-v1-server-protocol.h"
            "${CMAKE_CURRENT_BINARY_DIR}/ext-session-lock-v1-protocol.c")
        target_include_directories(tst_lock_protocol_integration PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/tests" "${CMAKE_CURRENT_BINARY_DIR}")
        target_link_libraries(tst_lock_protocol_integration PRIVATE qindaqt_lock_protocol
            PkgConfig::LockWaylandServer Qt6::Test Qt6::GuiPrivate Threads::Threads)
        target_compile_definitions(tst_lock_protocol_integration PRIVATE
            QINDAQT_PRIVATE_LOCK_PLUGIN_ROOT="${CMAKE_CURRENT_BINARY_DIR}/plugins")
        add_dependencies(tst_lock_protocol_integration qindaqt_session_lock_integration)
        qindaqt_enable_warnings(tst_lock_protocol_integration)
        add_test(NAME lock.protocol-integration COMMAND tst_lock_protocol_integration)
        # The skipped configures log a warning on purpose and the test verifies it.
        set_tests_properties(lock.protocol-integration PROPERTIES
            ENVIRONMENT "QT_FATAL_WARNINGS=0" TIMEOUT 60 SKIP_RETURN_CODE 77 LABELS "lock;protocol")
    endif()
endif()

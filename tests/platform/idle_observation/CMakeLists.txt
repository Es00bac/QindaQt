# SPDX-License-Identifier: GPL-3.0-or-later
find_package(Wayland REQUIRED COMPONENTS Server)
find_package(PkgConfig REQUIRED)
pkg_get_variable(QINDAQT_IDLE_PROTOCOL_DATA wayland-protocols pkgdatadir)
set(QINDAQT_IDLE_XML "${QINDAQT_IDLE_PROTOCOL_DATA}/staging/ext-idle-notify/ext-idle-notify-v1.xml")
find_program(QINDAQT_IDLE_SCANNER wayland-scanner REQUIRED)
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/ext-idle-notify-server.h"
    COMMAND ${QINDAQT_IDLE_SCANNER} server-header ${QINDAQT_IDLE_XML} "${CMAKE_CURRENT_BINARY_DIR}/ext-idle-notify-server.h"
    DEPENDS ${QINDAQT_IDLE_XML} VERBATIM)
qt_add_executable(qindaqt_wayland_idle_observation_tests tst_wayland_idle_observation.cpp idle_fixture_server.h
    "${CMAKE_CURRENT_BINARY_DIR}/ext-idle-notify-server.h")
target_include_directories(qindaqt_wayland_idle_observation_tests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(qindaqt_wayland_idle_observation_tests PRIVATE QindaQt::IdleObservation Wayland::Server Qt6::Test)
qindaqt_enable_warnings(qindaqt_wayland_idle_observation_tests)
add_test(NAME qindaqt.wayland-idle-observation COMMAND qindaqt_wayland_idle_observation_tests)

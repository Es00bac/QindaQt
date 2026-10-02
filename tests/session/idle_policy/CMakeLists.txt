# SPDX-License-Identifier: GPL-3.0-or-later
qt_add_executable(qindaqt_idle_display_stage_tests tst_display_off_stage.cpp)
target_link_libraries(qindaqt_idle_display_stage_tests
    PRIVATE QindaQt::IdlePolicy Qt6::Test)
qindaqt_enable_warnings(qindaqt_idle_display_stage_tests)
add_test(NAME qindaqt.session-idle-display-stage
    COMMAND qindaqt_idle_display_stage_tests)
set_tests_properties(qindaqt.session-idle-display-stage PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    LABELS "session;idle;display-power;model")

find_package(PlasmaWaylandProtocols REQUIRED CONFIG)
find_package(Wayland REQUIRED COMPONENTS Server)
find_program(QINDAQT_DPMS_SCANNER wayland-scanner REQUIRED)
set(QINDAQT_DPMS_XML "${PLASMA_WAYLAND_PROTOCOLS_DIR}/dpms.xml")
set(QINDAQT_DPMS_SERVER_HEADER "${CMAKE_CURRENT_BINARY_DIR}/dpms-server-protocol.h")
add_custom_command(OUTPUT "${QINDAQT_DPMS_SERVER_HEADER}"
    COMMAND "${QINDAQT_DPMS_SCANNER}" server-header "${QINDAQT_DPMS_XML}" "${QINDAQT_DPMS_SERVER_HEADER}"
    DEPENDS "${QINDAQT_DPMS_XML}" VERBATIM)
qt_add_executable(qindaqt_kwayland_dpms_controller_tests
    tst_kwayland_dpms_controller.cpp dpms_wayland_fixture.cpp dpms_wayland_fixture.h
    "${QINDAQT_DPMS_SERVER_HEADER}")
target_include_directories(qindaqt_kwayland_dpms_controller_tests PRIVATE
    "${CMAKE_CURRENT_BINARY_DIR}"
    "${PROJECT_SOURCE_DIR}/src/session/idle_policy/production")
target_link_libraries(qindaqt_kwayland_dpms_controller_tests
    PRIVATE QindaQt::IdlePolicy Wayland::Server Qt6::Test)
qindaqt_enable_warnings(qindaqt_kwayland_dpms_controller_tests)
add_test(NAME qindaqt.kwayland-dpms-controller
    COMMAND qindaqt_kwayland_dpms_controller_tests)
set_tests_properties(qindaqt.kwayland-dpms-controller PROPERTIES
    ENVIRONMENT "QT_FATAL_WARNINGS=1"
    LABELS "session;idle;display-power;wayland;private-fixture")

qt_add_executable(qindaqt_shared_idle_stage_tests tst_shared_idle_stages.cpp)
target_link_libraries(qindaqt_shared_idle_stage_tests PRIVATE QindaQt::IdlePolicy Qt6::Test)
qindaqt_enable_warnings(qindaqt_shared_idle_stage_tests)
add_test(NAME qindaqt.shared-idle-stages COMMAND qindaqt_shared_idle_stage_tests)
set_tests_properties(qindaqt.shared-idle-stages PROPERTIES LABELS "session;idle;power;model" TIMEOUT 30)

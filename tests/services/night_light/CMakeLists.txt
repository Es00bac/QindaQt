# SPDX-License-Identifier: LGPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Core DBus Test)

qt_add_executable(qindaqt_night_light_values_tests tst_night_light_values.cpp)
target_link_libraries(qindaqt_night_light_values_tests
                      PRIVATE QindaQt::NightLight Qt6::Core Qt6::Test)
qindaqt_enable_warnings(qindaqt_night_light_values_tests)
add_test(NAME qindaqt.night-light-values
         COMMAND qindaqt_night_light_values_tests)
set_tests_properties(qindaqt.night-light-values
                     PROPERTIES LABELS "service;night-light;display")

qt_add_executable(qindaqt_night_light_config_port_tests
                  tst_night_light_config_port.cpp)
target_link_libraries(qindaqt_night_light_config_port_tests
                      PRIVATE QindaQt::NightLight Qt6::Core Qt6::Test)
qindaqt_enable_warnings(qindaqt_night_light_config_port_tests)
add_test(NAME qindaqt.night-light-config-port
         COMMAND qindaqt_night_light_config_port_tests)
set_tests_properties(qindaqt.night-light-config-port
                     PROPERTIES LABELS "service;night-light;display")

find_program(DBUS_DAEMON_EXECUTABLE dbus-daemon REQUIRED)
qt_add_executable(qindaqt_night_light_state_port_tests
                  tst_night_light_state_port.cpp)
target_link_libraries(qindaqt_night_light_state_port_tests
                      PRIVATE QindaQt::NightLight Qt6::Core Qt6::DBus Qt6::Test)
target_compile_definitions(
    qindaqt_night_light_state_port_tests PRIVATE
    QINDAQT_DBUS_DAEMON_EXECUTABLE="${DBUS_DAEMON_EXECUTABLE}"
)
qindaqt_enable_warnings(qindaqt_night_light_state_port_tests)
add_test(NAME qindaqt.night-light-state-port
         COMMAND qindaqt_night_light_state_port_tests)
set_tests_properties(qindaqt.night-light-state-port
                     PROPERTIES LABELS "service;night-light;display")

add_test(
    NAME qindaqt.night-light-boundary
    COMMAND
        "${CMAKE_COMMAND}"
        "-DSOURCE_ROOT=${CMAKE_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/check_boundary.cmake"
)
set_tests_properties(qindaqt.night-light-boundary
                     PROPERTIES LABELS "service;night-light;display;policy")

# Poison negative: proves the boundary policy above is not vacuous by
# planting a forbidden dependency in a disposable copy and requiring the
# check to reject it.
add_test(
    NAME qindaqt.night-light-boundary-poison
    COMMAND
        "${CMAKE_COMMAND}"
        "-DCHECK_SCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/check_boundary.cmake"
        "-DMODULE_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../../../src/services/night_light"
        "-DSTAGE_DIR=${CMAKE_CURRENT_BINARY_DIR}/night-light-boundary-poison-stage"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/check_boundary_negative.cmake"
)
set_tests_properties(qindaqt.night-light-boundary-poison
                     PROPERTIES LABELS "service;night-light;display;policy")

# Private-proof helper (no test registration): the ADR-0136 headless KWin
# proof writes night light settings through the production config port and
# holds an inhibit lock with a live caller connection. It runs only through
# qq-private, never as a ctest row.
qt_add_executable(qindaqt_night_light_proof_helper
                  proof/night_light_proof_helper.cpp)
target_link_libraries(qindaqt_night_light_proof_helper
                      PRIVATE QindaQt::NightLight Qt6::Core Qt6::DBus)
qindaqt_enable_warnings(qindaqt_night_light_proof_helper)

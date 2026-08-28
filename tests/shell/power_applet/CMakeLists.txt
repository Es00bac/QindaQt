# SPDX-License-Identifier: GPL-3.0-or-later

# AGENT-NOTE: worker-owned P1 candidate; not yet wired into tests/CMakeLists.txt.
# Integration is one additive `add_subdirectory(shell/power_applet)` line there
# plus the manager's registry seams named in the P1 handoff.

qt_add_executable(qindaqt_power_applet_presentation_tests
                  tst_power_applet_presentation.cpp)
target_link_libraries(
    qindaqt_power_applet_presentation_tests
    PRIVATE QindaQt::ShellPowerApplet Qt6::Test
)
qindaqt_enable_warnings(qindaqt_power_applet_presentation_tests)
add_test(
    NAME qindaqt.power-applet-presentation
    COMMAND qindaqt_power_applet_presentation_tests
)
set_tests_properties(
    qindaqt.power-applet-presentation PROPERTIES LABELS "shell;power;pure"
)

qt_add_executable(qindaqt_power_applet_controls_tests
                  tst_power_applet_controls.cpp)
target_link_libraries(
    qindaqt_power_applet_controls_tests
    PRIVATE QindaQt::ShellPowerApplet Qt6::Test
)
qindaqt_enable_warnings(qindaqt_power_applet_controls_tests)
add_test(
    NAME qindaqt.power-applet-control-rows
    COMMAND qindaqt_power_applet_controls_tests
)
set_tests_properties(
    qindaqt.power-applet-control-rows PROPERTIES LABELS "shell;power;pure"
)

qt_add_executable(qindaqt_power_applet_request_tests
                  tst_brightness_request_state.cpp)
target_link_libraries(
    qindaqt_power_applet_request_tests
    PRIVATE QindaQt::ShellPowerApplet Qt6::Test
)
qindaqt_enable_warnings(qindaqt_power_applet_request_tests)
add_test(
    NAME qindaqt.power-applet-request-state
    COMMAND qindaqt_power_applet_request_tests
)
set_tests_properties(
    qindaqt.power-applet-request-state PROPERTIES LABELS "shell;power;pure"
)

add_test(
    NAME qindaqt.power-applet-boundary
    COMMAND
        "${CMAKE_COMMAND}"
        "-DSOURCE_ROOT=${CMAKE_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/check_boundary.cmake"
)
set_tests_properties(
    qindaqt.power-applet-boundary PROPERTIES LABELS "shell;power;pure;policy"
)

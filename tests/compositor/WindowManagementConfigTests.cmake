# SPDX-License-Identifier: GPL-3.0-or-later
#
# AGENT-NOTE: its own file because tests/compositor/CMakeLists.txt is at
# the 600-line shape limit (see ContainerShadeTests.cmake).

# The compositor half of the windowManagement.* bridge (ADR-0209): the
# kwinrc [QindaQt] spellings decode to the docking chord and close policy.
# Pure Qt: the parser deliberately carries no KConfig or KWin dependency.
qt_add_executable(
    qindaqt_window_management_config_tests
    tst_windowmanagementconfig.cpp
    "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin/windowmanagementconfig.cpp"
)
target_include_directories(
    qindaqt_window_management_config_tests
    PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin"
)
target_link_libraries(
    qindaqt_window_management_config_tests
    PRIVATE Qt6::Core Qt6::Test
)
if(COMMAND qindaqt_enable_warnings)
    qindaqt_enable_warnings(qindaqt_window_management_config_tests)
endif()
add_test(
    NAME compositor.window-management-config
    COMMAND qindaqt_window_management_config_tests
)

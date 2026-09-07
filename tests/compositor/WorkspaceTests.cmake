# SPDX-License-Identifier: GPL-3.0-or-later
# Workspace adoption policy tests share the compositor test directory scope.
qt_add_executable(
    qindaqt_kwin_workspace_ui_port_policy_tests
    tst_kwinworkspaceuiportpolicy.cpp
)
target_include_directories(
    qindaqt_kwin_workspace_ui_port_policy_tests
    PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin"
)
target_link_libraries(
    qindaqt_kwin_workspace_ui_port_policy_tests
    PRIVATE QindaQt::WorkspacesUi Qt6::Test
)
add_test(
    NAME compositor.kwin-workspace-ui-port-policy
    COMMAND qindaqt_kwin_workspace_ui_port_policy_tests
)

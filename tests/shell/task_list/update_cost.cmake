# SPDX-License-Identifier: GPL-3.0-or-later
#
# Included from tests/shell/task_list/CMakeLists.txt (same directory scope, so it
# uses that file's qindaqt_add_task_list_applet_test and boundary variables).
# Kept apart so the already large parent stays within the source-shape policy.

# Stability round 2026-10-16 (L3): per-update cost of the task list. One title
# change on a busy strip must touch exactly one row; the harness also prints
# thread CPU time per update over the host's real desktop entries and icon
# themes, so the before/after cost stays measurable without a wall-clock bound.
qindaqt_add_task_list_applet_test(
    qindaqt_task_list_update_cost_tests
    tst_task_list_update_cost.cpp
    qindaqt.task-list-update-cost
    "shell;task-list;offscreen;perf"
    task_list_applet_test_fakes.h
    task_list_operation_test_support.h
)
target_link_libraries(
    qindaqt_task_list_update_cost_tests
    PRIVATE
        QindaQt::TokensQml
        qindaqt_shell_task_list_appletplugin
        QindaQt::ShellIcons
        qindaqt_shell_iconsplugin
        qindaqt_themes
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
)
target_compile_definitions(
    qindaqt_task_list_update_cost_tests
    PRIVATE
        QINDAQT_TASK_LIST_APPLET_QML_IMPORT_PATH="${CMAKE_BINARY_DIR}/qml"
        QINDAQT_SOURCE_DIR="${task_list_boundary_source_root}"
)
add_dependencies(
    qindaqt_task_list_update_cost_tests
    qindaqt_tokens_qmlplugin
    qindaqt_controls_qmlplugin
    qindaqt_shell_iconsplugin
)
qt_import_qml_plugins(qindaqt_task_list_update_cost_tests)
# AGENT-NOTE: no QT_FATAL_WARNINGS here, unlike the QML rows above. This
# harness renders the host's real icon themes, and a third-party SVG with bad
# path data makes Qt warn; that must not abort a measurement.
set_tests_properties(
    qindaqt.task-list-update-cost
    PROPERTIES
        ENVIRONMENT
            "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QML2_IMPORT_PATH=${CMAKE_BINARY_DIR}/qml"
        TIMEOUT 300
)

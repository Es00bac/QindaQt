# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_executable(
    qindaqt_panel_interaction_store_tests
    tst_panel_interaction_store.cpp
)
target_link_libraries(
    qindaqt_panel_interaction_store_tests
    PRIVATE QindaQt::ShellOrchestration Qt6::Test
)
qindaqt_enable_warnings(qindaqt_panel_interaction_store_tests)
add_test(
    NAME qindaqt.shell-orchestration-interactions
    COMMAND qindaqt_panel_interaction_store_tests
)

qt_add_executable(
    qindaqt_panel_visibility_inventory_assembler_tests
    tst_panel_visibility_inventory_assembler.cpp
)
target_link_libraries(
    qindaqt_panel_visibility_inventory_assembler_tests
    PRIVATE QindaQt::ShellOrchestration Qt6::Test
)
qindaqt_enable_warnings(qindaqt_panel_visibility_inventory_assembler_tests)
add_test(
    NAME qindaqt.shell-orchestration-visibility-inventory
    COMMAND qindaqt_panel_visibility_inventory_assembler_tests
)

qt_add_executable(
    qindaqt_panel_runtime_plan_assembler_tests
    tst_panel_runtime_plan_assembler.cpp
)
target_link_libraries(
    qindaqt_panel_runtime_plan_assembler_tests
    PRIVATE QindaQt::ShellOrchestration Qt6::Test
)
qindaqt_enable_warnings(qindaqt_panel_runtime_plan_assembler_tests)
add_test(
    NAME qindaqt.shell-orchestration-runtime-plan
    COMMAND qindaqt_panel_runtime_plan_assembler_tests
)

qt_add_executable(
    qindaqt_output_inventory_matcher_tests
    tst_output_inventory_matcher.cpp
)
target_link_libraries(
    qindaqt_output_inventory_matcher_tests
    PRIVATE QindaQt::ShellOrchestration Qt6::Test
)
qindaqt_enable_warnings(qindaqt_output_inventory_matcher_tests)
add_test(
    NAME qindaqt.shell-orchestration-output-match
    COMMAND qindaqt_output_inventory_matcher_tests
)

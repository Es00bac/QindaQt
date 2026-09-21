# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_executable(
    qindaqt_gather_overview_projection_tests tst_gather_overview_projection.cpp
)
target_link_libraries(
    qindaqt_gather_overview_projection_tests
    PRIVATE QindaQt::ShellGatherOverview Qt6::Test
)
set_target_properties(
    qindaqt_gather_overview_projection_tests
    PROPERTIES AUTOMOC ON CXX_EXTENSIONS OFF
)
qindaqt_enable_warnings(qindaqt_gather_overview_projection_tests)
add_test(
    NAME qindaqt.gather-overview-projection
    COMMAND qindaqt_gather_overview_projection_tests
)
set_tests_properties(
    qindaqt.gather-overview-projection
    PROPERTIES TIMEOUT 20 LABELS "unit;pure;shell"
)

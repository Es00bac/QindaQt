# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_executable(qindaqt_display_topology_geometry_tests tst_topology_geometry.cpp)
target_link_libraries(
    qindaqt_display_topology_geometry_tests
    PRIVATE QindaQt::DisplayTopology Qt6::Test
)
qindaqt_enable_warnings(qindaqt_display_topology_geometry_tests)
add_test(NAME qindaqt.display-topology-geometry COMMAND qindaqt_display_topology_geometry_tests)
set_tests_properties(qindaqt.display-topology-geometry PROPERTIES LABELS "service;display")

qt_add_executable(qindaqt_display_topology_candidate_tests tst_topology_candidate.cpp)
target_link_libraries(
    qindaqt_display_topology_candidate_tests
    PRIVATE QindaQt::DisplayTopology Qt6::Test
)
qindaqt_enable_warnings(qindaqt_display_topology_candidate_tests)
add_test(NAME qindaqt.display-topology-candidate COMMAND qindaqt_display_topology_candidate_tests)
set_tests_properties(qindaqt.display-topology-candidate PROPERTIES LABELS "service;display")

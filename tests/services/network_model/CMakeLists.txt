# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_executable(qindaqt_network_snapshot_gate_tests tst_network_snapshot_gate.cpp)
target_include_directories(qindaqt_network_snapshot_gate_tests
                           PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../network_protocol")
target_link_libraries(qindaqt_network_snapshot_gate_tests
                      PRIVATE QindaQt::NetworkModel Qt6::Test)
qindaqt_enable_warnings(qindaqt_network_snapshot_gate_tests)
add_test(NAME qindaqt.network-model-snapshot-gate
         COMMAND qindaqt_network_snapshot_gate_tests)
set_tests_properties(qindaqt.network-model-snapshot-gate
                     PROPERTIES LABELS "service;network;pure")

qt_add_executable(qindaqt_network_scan_lease_tests tst_network_scan_lease.cpp)
target_include_directories(qindaqt_network_scan_lease_tests
                           PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../network_protocol")
target_link_libraries(qindaqt_network_scan_lease_tests
                      PRIVATE QindaQt::NetworkModel Qt6::Test)
qindaqt_enable_warnings(qindaqt_network_scan_lease_tests)
add_test(NAME qindaqt.network-model-scan-lease
         COMMAND qindaqt_network_scan_lease_tests)
set_tests_properties(qindaqt.network-model-scan-lease
                     PROPERTIES LABELS "service;network;pure")

qt_add_executable(qindaqt_network_intent_policy_tests tst_network_intent_policy.cpp)
target_include_directories(qindaqt_network_intent_policy_tests
                           PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../network_protocol")
target_link_libraries(qindaqt_network_intent_policy_tests
                      PRIVATE QindaQt::NetworkModel Qt6::Test)
qindaqt_enable_warnings(qindaqt_network_intent_policy_tests)
add_test(NAME qindaqt.network-model-intent-policy
         COMMAND qindaqt_network_intent_policy_tests)
set_tests_properties(qindaqt.network-model-intent-policy
                     PROPERTIES LABELS "service;network;pure")

qt_add_executable(qindaqt_network_model_tests tst_network_model.cpp)
target_include_directories(qindaqt_network_model_tests
                           PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../network_protocol")
target_link_libraries(qindaqt_network_model_tests
                      PRIVATE QindaQt::NetworkModel Qt6::Test)
qindaqt_enable_warnings(qindaqt_network_model_tests)
add_test(NAME qindaqt.network-model-state COMMAND qindaqt_network_model_tests)
set_tests_properties(qindaqt.network-model-state
                     PROPERTIES LABELS "service;network;pure")

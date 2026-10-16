# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_executable(qindaqt_applet_host_policy_tests tst_host_policy.cpp)
target_link_libraries(qindaqt_applet_host_policy_tests PRIVATE qindaqt_applet_host Qt6::Test)
target_compile_definitions(
    qindaqt_applet_host_policy_tests
    PRIVATE QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)
qindaqt_enable_warnings(qindaqt_applet_host_policy_tests)
add_test(NAME qindaqt.applet-host-policy COMMAND qindaqt_applet_host_policy_tests)

qt_add_executable(qindaqt_applet_host_handshake_tests tst_handshake.cpp)
target_link_libraries(qindaqt_applet_host_handshake_tests PRIVATE qindaqt_applet_host Qt6::Test)
target_compile_definitions(
    qindaqt_applet_host_handshake_tests
    PRIVATE QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)
qindaqt_enable_warnings(qindaqt_applet_host_handshake_tests)
add_test(NAME qindaqt.applet-host-handshake COMMAND qindaqt_applet_host_handshake_tests)

qt_add_executable(qindaqt_applet_host_lifecycle_tests tst_lifecycle.cpp)
target_link_libraries(qindaqt_applet_host_lifecycle_tests PRIVATE qindaqt_applet_host Qt6::Test)
qindaqt_enable_warnings(qindaqt_applet_host_lifecycle_tests)
add_test(NAME qindaqt.applet-host-lifecycle COMMAND qindaqt_applet_host_lifecycle_tests)

# ADR-0393: the shell merges every XDG applet-policy file.
qt_add_executable(qindaqt_applet_host_policy_merge_tests tst_policy_merge.cpp)
target_link_libraries(qindaqt_applet_host_policy_merge_tests PRIVATE qindaqt_applet_host Qt6::Test)
target_compile_definitions(
    qindaqt_applet_host_policy_merge_tests
    PRIVATE QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)
qindaqt_enable_warnings(qindaqt_applet_host_policy_merge_tests)
add_test(NAME qindaqt.applet-host-policy-merge COMMAND qindaqt_applet_host_policy_merge_tests)

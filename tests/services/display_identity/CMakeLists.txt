# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_executable(qindaqt_display_identity_resolver_tests tst_identity_resolver.cpp)
target_link_libraries(
    qindaqt_display_identity_resolver_tests PRIVATE QindaQt::DisplayIdentity Qt6::Test
)
qindaqt_enable_warnings(qindaqt_display_identity_resolver_tests)
add_test(NAME qindaqt.display-identity-resolver COMMAND qindaqt_display_identity_resolver_tests)
set_tests_properties(qindaqt.display-identity-resolver PROPERTIES LABELS "service;display")

qt_add_executable(qindaqt_display_identity_registry_tests tst_identity_registry.cpp)
target_link_libraries(
    qindaqt_display_identity_registry_tests PRIVATE QindaQt::DisplayIdentity Qt6::Test
)
qindaqt_enable_warnings(qindaqt_display_identity_registry_tests)
add_test(NAME qindaqt.display-identity-registry COMMAND qindaqt_display_identity_registry_tests)
set_tests_properties(qindaqt.display-identity-registry PROPERTIES LABELS "service;display")

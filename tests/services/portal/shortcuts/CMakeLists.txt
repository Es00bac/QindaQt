# SPDX-License-Identifier: GPL-3.0-or-later
qt_add_executable(qindaqt_portal_shortcuts_policy_tests tst_shortcut_policy.cpp)
target_link_libraries(qindaqt_portal_shortcuts_policy_tests PRIVATE QindaQt::PortalShortcutsPolicy Qt6::Test)
qindaqt_enable_warnings(qindaqt_portal_shortcuts_policy_tests)
add_test(NAME qindaqt.portal-shortcuts-policy COMMAND qindaqt_portal_shortcuts_policy_tests)
set_tests_properties(qindaqt.portal-shortcuts-policy PROPERTIES LABELS "portal;shortcuts" TIMEOUT 30)

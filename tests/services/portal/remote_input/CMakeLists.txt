# SPDX-License-Identifier: GPL-3.0-or-later
qt_add_executable(qindaqt_portal_remote_desktop_tests tst_remote_desktop.cpp)
target_link_libraries(qindaqt_portal_remote_desktop_tests PRIVATE QindaQt::PortalRemoteInput Qt6::Test)
qindaqt_enable_warnings(qindaqt_portal_remote_desktop_tests)
add_test(NAME qindaqt.portal-remote-desktop COMMAND qindaqt_portal_remote_desktop_tests)
set_tests_properties(qindaqt.portal-remote-desktop PROPERTIES TIMEOUT 45 LABELS "portal;private-bus")

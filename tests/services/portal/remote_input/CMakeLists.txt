# SPDX-License-Identifier: GPL-3.0-or-later
foreach(family remote_desktop input_capture)
    qt_add_executable(qindaqt_portal_${family}_tests tst_${family}.cpp remote_input_fixture.h)
    target_link_libraries(qindaqt_portal_${family}_tests PRIVATE QindaQt::PortalRemoteInput Qt6::Test)
    qindaqt_enable_warnings(qindaqt_portal_${family}_tests)
endforeach()
add_test(NAME qindaqt.portal-remote-desktop COMMAND qindaqt_portal_remote_desktop_tests)
add_test(NAME qindaqt.portal-input-capture COMMAND qindaqt_portal_input_capture_tests)
set_tests_properties(qindaqt.portal-remote-desktop qindaqt.portal-input-capture
    PROPERTIES TIMEOUT 45 LABELS "portal;private-bus")

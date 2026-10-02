# SPDX-License-Identifier: GPL-3.0-or-later
foreach(family remote_desktop input_capture remote_screencast)
    qt_add_executable(qindaqt_portal_${family}_tests tst_${family}.cpp remote_input_fixture.h)
    target_link_libraries(qindaqt_portal_${family}_tests PRIVATE QindaQt::PortalRemoteInput Qt6::Test)
    qindaqt_enable_warnings(qindaqt_portal_${family}_tests)
endforeach()
add_test(NAME qindaqt.portal-remote-desktop COMMAND qindaqt_portal_remote_desktop_tests)
add_test(NAME qindaqt.portal-input-capture COMMAND qindaqt_portal_input_capture_tests)
add_test(NAME qindaqt.portal-remote-screencast COMMAND qindaqt_portal_remote_screencast_tests)
set_tests_properties(qindaqt.portal-remote-desktop qindaqt.portal-input-capture qindaqt.portal-remote-screencast
    PROPERTIES TIMEOUT 45 LABELS "portal;private-bus")
# A real libeis server observes the legacy Notify* events the sender emits.
find_package(PkgConfig REQUIRED)
pkg_check_modules(QINDAQT_TEST_LIBEIS REQUIRED IMPORTED_TARGET libeis-1.0)
qt_add_executable(qindaqt_portal_remote_legacy_input_tests tst_remote_legacy_input.cpp remote_input_fixture.h)
target_link_libraries(qindaqt_portal_remote_legacy_input_tests PRIVATE QindaQt::PortalRemoteInput PkgConfig::QINDAQT_TEST_LIBEIS Qt6::Test)
qindaqt_enable_warnings(qindaqt_portal_remote_legacy_input_tests)
add_test(NAME qindaqt.portal-remote-legacy-input COMMAND qindaqt_portal_remote_legacy_input_tests)
set_tests_properties(qindaqt.portal-remote-legacy-input PROPERTIES TIMEOUT 45 LABELS "portal;private-bus")

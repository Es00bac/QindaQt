# SPDX-License-Identifier: GPL-3.0-or-later
find_package(Wayland REQUIRED COMPONENTS Client)
qt_add_executable(qindaqt_portal_capture_policy_tests tst_capture_policy.cpp)
target_link_libraries(qindaqt_portal_capture_policy_tests PRIVATE QindaQt::PortalCapturePolicy Qt6::Test)
qindaqt_enable_warnings(qindaqt_portal_capture_policy_tests)
add_test(NAME qindaqt.portal-capture-policy COMMAND qindaqt_portal_capture_policy_tests)
qt_add_executable(qindaqt_portal_capture_request_tests tst_capture_requests.cpp)
target_link_libraries(qindaqt_portal_capture_request_tests PRIVATE QindaQt::PortalCapture Qt6::Test)
qindaqt_enable_warnings(qindaqt_portal_capture_request_tests)
add_test(NAME qindaqt.portal-capture-requests COMMAND qindaqt_portal_capture_request_tests)
set_tests_properties(qindaqt.portal-capture-policy qindaqt.portal-capture-requests
    PROPERTIES TIMEOUT 45 LABELS "portal;capture;private-bus;hostile")

# AGENT-CONTRACT: private input drives the same native GUI and compositor ports;
# it never bypasses admission or fabricates capture pixels/PipeWire node results.
set(capture_dir "${PROJECT_SOURCE_DIR}/src/services/portal/capture")
qt_add_executable(qindaqt_portal_capture_input "${capture_dir}/main.cpp"
    "${capture_dir}/native_capture_admission.cpp" "${capture_dir}/native_capture_admission.h"
    "${capture_dir}/capture_dialog.cpp" "${capture_dir}/capture_dialog.h"
    "${capture_dir}/color_picker.cpp" "${capture_dir}/color_picker.h" capture_input.cpp)
target_link_libraries(qindaqt_portal_capture_input PRIVATE QindaQt::PortalCapturePolicy
    QindaQt::CompositorCapture QindaQt::WaylandScreenCast QindaQt::SessionLockState
    QindaQt::ForeignParent Qt6::Widgets Qt6::Test Wayland::Client)
qindaqt_enable_warnings(qindaqt_portal_capture_input)

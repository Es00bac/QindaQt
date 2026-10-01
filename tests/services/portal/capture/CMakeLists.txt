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

# Protected native fixtures need the selected installed public fork contract;
# absence keeps ordinary policy/request tests but advertises no native gate.
if(NOT TARGET qindaqt_portal_capture_authority)
    return()
endif()
qt_add_executable(qindaqt_portal_capture_authority_packet_tests tst_authority_packet.cpp)
target_include_directories(qindaqt_portal_capture_authority_packet_tests PRIVATE "${PROJECT_SOURCE_DIR}/src/services/portal/capture")
target_link_libraries(qindaqt_portal_capture_authority_packet_tests PRIVATE qindaqt_portal_capture_authority Qt6::Test)
qindaqt_enable_warnings(qindaqt_portal_capture_authority_packet_tests)
add_test(NAME qindaqt.portal-capture-authority-packet COMMAND qindaqt_portal_capture_authority_packet_tests)
set_tests_properties(qindaqt.portal-capture-authority-packet PROPERTIES TIMEOUT 30 LABELS "portal;capture;wire;hostile")

# AGENT-CONTRACT: private input drives the same native GUI and compositor ports;
# it never bypasses admission or fabricates capture pixels/PipeWire node results.
set(capture_dir "${PROJECT_SOURCE_DIR}/src/services/portal/capture")
qt_add_executable(qindaqt_portal_capture_input "${capture_dir}/main.cpp"
    "${capture_dir}/helper_runtime.cpp" "${capture_dir}/helper_runtime.h"
    "${capture_dir}/native_capture_admission.cpp" "${capture_dir}/native_capture_admission.h"
    "${capture_dir}/capture_dialog.cpp" "${capture_dir}/capture_dialog.h"
    "${capture_dir}/color_picker.cpp" "${capture_dir}/color_picker.h" capture_input.cpp)
target_link_libraries(qindaqt_portal_capture_input PRIVATE QindaQt::PortalCapturePolicy
    QindaQt::CompositorCapture QindaQt::WaylandScreenCast QindaQt::SessionLockState
    QindaQt::ForeignParent Qt6::Widgets Qt6::Test Wayland::Client qindaqt_portal_capture_authority)
target_compile_definitions(qindaqt_portal_capture_input PRIVATE QINDAQT_CAPTURE_ACTUAL_INPUT_TEST=1)
qindaqt_enable_warnings(qindaqt_portal_capture_input)

# Separate test broker links the unchanged protected policy with a compile-only
# per-job input directive. Neither this executable nor its variants install.
qt_add_library(qindaqt_portal_capture_test_broker STATIC
    "${capture_dir}/backend/authority_capture.cpp" "${capture_dir}/backend/authority_capture.h"
    "${capture_dir}/native_capture_admission.cpp" "${capture_dir}/native_capture_admission.h" backend_input_control.cpp)
target_compile_definitions(qindaqt_portal_capture_test_broker PRIVATE QINDAQT_CAPTURE_ACTUAL_INPUT_TEST=1 QT_NO_KEYWORDS)
target_link_libraries(qindaqt_portal_capture_test_broker PUBLIC QindaQt::PortalCapture
    QindaQt::SessionLockState qindaqt_portal_capture_authority PRIVATE QindaQt::CompositorNames)
qindaqt_enable_warnings(qindaqt_portal_capture_test_broker)
qt_add_executable(qindaqt_portal_capture_test_backend "${capture_dir}/backend/main.cpp")
target_link_libraries(qindaqt_portal_capture_test_backend PRIVATE qindaqt_portal_capture_test_broker)
qindaqt_enable_warnings(qindaqt_portal_capture_test_backend)

qt_add_executable(qindaqt_portal_capture_pixels capture_pixels.cpp)
target_link_libraries(qindaqt_portal_capture_pixels PRIVATE Qt6::Widgets)
qindaqt_enable_warnings(qindaqt_portal_capture_pixels)

find_package(PkgConfig REQUIRED)
pkg_check_modules(CAPTURE_TEST_PIPEWIRE REQUIRED IMPORTED_TARGET libpipewire-0.3)
qt_add_executable(qindaqt_portal_native_capture_tests tst_native_capture.cpp pipewire_frames.cpp pipewire_frames.h)
target_link_libraries(qindaqt_portal_native_capture_tests PRIVATE QindaQt::PortalComposition
    QindaQt::PortalService QindaQt::CompositorNames Qt6::Widgets Qt6::Test PkgConfig::CAPTURE_TEST_PIPEWIRE)
target_compile_definitions(qindaqt_portal_native_capture_tests PRIVATE
    QINDAQT_FRONTEND_EXECUTABLE="${QINDAQT_XDG_DESKTOP_PORTAL_EXECUTABLE}")
qindaqt_enable_warnings(qindaqt_portal_native_capture_tests)
add_dependencies(qindaqt_portal_native_capture_tests qindaqt_portal_capture_input
    qindaqt-portal-capture qindaqt_portal_capture_test_backend qindaqt-portal-consent qindaqt-uri-relay qindaqt_portal_foreign_exporter qindaqt_portal_capture_pixels)
add_test(NAME qindaqt.portal-native-capture COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/run_native_capture.py"
    "$<TARGET_FILE:qindaqt_portal_native_capture_tests>" "$<TARGET_FILE:qindaqt_portal_capture_input>"
    "${QINDAQT_KWIN_WAYLAND}" "$<TARGET_FILE:qindaqt-portal-consent>" "$<TARGET_FILE:qindaqt-uri-relay>"
    "$<TARGET_FILE:qindaqt_portal_foreign_exporter>" "$<TARGET_FILE:qindaqt_portal_capture_pixels>"
    "${PROJECT_SOURCE_DIR}/src/services/portal/data/qindaqt.portal"
    "${PROJECT_SOURCE_DIR}/src/services/portal/data/qindaqt-portals.conf")
set_tests_properties(qindaqt.portal-native-capture PROPERTIES TIMEOUT 420 SKIP_RETURN_CODE 77
    RUN_SERIAL TRUE LABELS "portal;capture;frontend;private-wayland;private-pipewire;privacy")

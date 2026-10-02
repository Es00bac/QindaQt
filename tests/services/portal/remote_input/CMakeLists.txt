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
# Test-only closure between real libei setup and first dispatch; no production hook.
target_link_options(qindaqt_portal_remote_legacy_input_tests PRIVATE "LINKER:--wrap=ei_setup_backend_fd")
qindaqt_enable_warnings(qindaqt_portal_remote_legacy_input_tests)
add_test(NAME qindaqt.portal-remote-legacy-input COMMAND qindaqt_portal_remote_legacy_input_tests)
set_tests_properties(qindaqt.portal-remote-legacy-input PROPERTIES TIMEOUT 45 LABELS "portal;private-bus")

# Actual private journey (real frontend, mapped native consent, staged production
# compositor with a separately built candidate eis plugin). It skips with 77
# until QINDAQT_REMOTE_INPUT_EIS_PLUGIN_DIR names that candidate plugin tree.
find_package(PkgConfig REQUIRED)
pkg_check_modules(QINDAQT_LIBEI IMPORTED_TARGET libei-1.0)
# AGENT-CONTRACT: Both protected capture and remote-input journeys use this
# ordinary QtGui Wayland peer; its existence does not require the KWin plugin
# or supervisor fixture. Only the native remote-input journey below does.
qt_add_executable(qindaqt_portal_remote_input_clipboard_client native/clipboard_client.cpp)
target_link_libraries(qindaqt_portal_remote_input_clipboard_client PRIVATE Qt6::Gui)
qindaqt_enable_warnings(qindaqt_portal_remote_input_clipboard_client)
if(TARGET QindaQt::SessionSupervisorSupport AND QINDAQT_LIBEI_FOUND AND TARGET qindaqt_portal_consent_input)
    set(QINDAQT_REMOTE_INPUT_EIS_PLUGIN_DIR "" CACHE PATH "Candidate eis plugin tree for the private remote-input journey")
    qt_add_executable(qindaqt_portal_native_remote_input_tests native/tst_native_remote_input.cpp)
    add_dependencies(qindaqt_portal_native_remote_input_tests
        qindaqt_portal_remote_input_clipboard_client qindaqt_portal_consent_input)
    target_include_directories(qindaqt_portal_native_remote_input_tests PRIVATE "${PROJECT_SOURCE_DIR}")
    target_link_libraries(qindaqt_portal_native_remote_input_tests PRIVATE QindaQt::PortalService QindaQt::PortalComposition
        QindaQt::SecretPortal QindaQt::SessionSupervisorSupport QindaQt::CompositorNames PkgConfig::QINDAQT_LIBEI Qt6::Test)
    target_compile_definitions(qindaqt_portal_native_remote_input_tests PRIVATE
        QINDAQT_FRONTEND_EXECUTABLE="${QINDAQT_XDG_DESKTOP_PORTAL_EXECUTABLE}"
        QINDAQT_CLIPBOARD_CLIENT="$<TARGET_FILE:qindaqt_portal_remote_input_clipboard_client>")
    qindaqt_enable_warnings(qindaqt_portal_native_remote_input_tests)
    add_test(NAME qindaqt.portal-native-remote-input COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/native/run_native_remote_input.py" "$<TARGET_FILE:qindaqt_portal_native_remote_input_tests>"
        "$<TARGET_FILE:qindaqt_portal_consent_input>" "${QINDAQT_KWIN_WAYLAND}" "${QINDAQT_REMOTE_INPUT_EIS_PLUGIN_DIR}")
    set_tests_properties(qindaqt.portal-native-remote-input PROPERTIES TIMEOUT 180 SKIP_RETURN_CODE 77
        LABELS "portal;private-wayland")
endif()

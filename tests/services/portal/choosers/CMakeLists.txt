# SPDX-License-Identifier: GPL-3.0-or-later
find_package(Wayland REQUIRED COMPONENTS Client)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
qt_add_executable(qindaqt_portal_chooser_policy_tests tst_chooser_policy.cpp)
target_link_libraries(qindaqt_portal_chooser_policy_tests PRIVATE QindaQt::PortalChooserPolicy Qt6::Test)
qindaqt_enable_warnings(qindaqt_portal_chooser_policy_tests)
add_test(NAME qindaqt.portal-chooser-policy COMMAND qindaqt_portal_chooser_policy_tests)
qt_add_executable(qindaqt_portal_chooser_request_tests tst_chooser_requests.cpp)
target_link_libraries(qindaqt_portal_chooser_request_tests PRIVATE QindaQt::PortalChoosers Qt6::Test)
qindaqt_enable_warnings(qindaqt_portal_chooser_request_tests)
add_test(NAME qindaqt.portal-chooser-requests COMMAND qindaqt_portal_chooser_request_tests)
set_tests_properties(qindaqt.portal-chooser-policy qindaqt.portal-chooser-requests PROPERTIES TIMEOUT 45 LABELS "portal;choosers;private-bus")

set(chooser_dir "${PROJECT_SOURCE_DIR}/src/services/portal/choosers")
qt_add_executable(qindaqt_portal_chooser_input "${chooser_dir}/main.cpp" "${chooser_dir}/chooser_dialog.cpp" "${chooser_dir}/chooser_dialog.h"
    "${chooser_dir}/choice_controls.cpp" "${chooser_dir}/choice_controls.h" "${chooser_dir}/file_filter_proxy.cpp" "${chooser_dir}/file_filter_proxy.h"
    "${chooser_dir}/file_chooser_dialog.cpp" "${chooser_dir}/file_chooser_dialog.h" "${chooser_dir}/media_presenter.cpp" "${chooser_dir}/media_presenter.h"
    "${chooser_dir}/media_sidebar.cpp" "${chooser_dir}/media_sidebar.h" "${chooser_dir}/media_composition.cpp" "${chooser_dir}/media_composition.h"
    "${chooser_dir}/app_chooser_dialog.cpp" "${chooser_dir}/app_chooser_dialog.h" chooser_input.cpp)
target_link_libraries(qindaqt_portal_chooser_input PRIVATE QindaQt::PortalChooserPolicy QindaQt::ForeignParent QindaQt::RemovableMediaClient QindaQt::RemovableMediaLaunch Qt6::Widgets Qt6::Test Wayland::Client)
qindaqt_enable_warnings(qindaqt_portal_chooser_input)
qt_add_executable(qindaqt_portal_native_chooser_tests tst_native_choosers.cpp)
target_link_libraries(qindaqt_portal_native_chooser_tests PRIVATE QindaQt::PortalComposition QindaQt::PortalService QindaQt::CompositorNames Qt6::Test)
target_compile_definitions(qindaqt_portal_native_chooser_tests PRIVATE QINDAQT_FRONTEND_EXECUTABLE="${QINDAQT_XDG_DESKTOP_PORTAL_EXECUTABLE}")
qindaqt_enable_warnings(qindaqt_portal_native_chooser_tests)
add_dependencies(qindaqt_portal_native_chooser_tests qindaqt_portal_chooser_input
    qindaqt-portal-chooser qindaqt-portal-consent qindaqt-uri-relay qindaqt_portal_foreign_exporter)
add_test(NAME qindaqt.portal-native-choosers COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/run_native_choosers.py"
    "$<TARGET_FILE:qindaqt_portal_native_chooser_tests>" "$<TARGET_FILE:qindaqt_portal_chooser_input>" "${QINDAQT_KWIN_WAYLAND}"
    "$<TARGET_FILE:qindaqt-portal-consent>" "$<TARGET_FILE:qindaqt-uri-relay>" "$<TARGET_FILE:qindaqt_portal_foreign_exporter>"
    "${PROJECT_SOURCE_DIR}/src/services/portal/data/qindaqt.portal" "${PROJECT_SOURCE_DIR}/src/services/portal/data/qindaqt-portals.conf")
set_tests_properties(qindaqt.portal-native-choosers PROPERTIES TIMEOUT 180 SKIP_RETURN_CODE 77 RUN_SERIAL TRUE LABELS "portal;choosers;frontend;private-wayland")

qt_add_executable(qindaqt_portal_media_chooser_tests tst_media_chooser.cpp
    "${chooser_dir}/chooser_dialog.cpp" "${chooser_dir}/choice_controls.cpp"
    "${chooser_dir}/file_filter_proxy.cpp" "${chooser_dir}/file_chooser_dialog.cpp"
    "${chooser_dir}/media_presenter.cpp" "${chooser_dir}/media_presenter.h"
    "${chooser_dir}/media_sidebar.cpp" "${chooser_dir}/media_sidebar.h")
target_include_directories(qindaqt_portal_media_chooser_tests PRIVATE "${chooser_dir}")
target_link_libraries(qindaqt_portal_media_chooser_tests PRIVATE QindaQt::PortalChooserPolicy QindaQt::RemovableMediaClient Qt6::Widgets Qt6::Test)
qindaqt_enable_warnings(qindaqt_portal_media_chooser_tests)
add_test(NAME qindaqt.portal-media-chooser COMMAND qindaqt_portal_media_chooser_tests)
set_tests_properties(qindaqt.portal-media-chooser PROPERTIES TIMEOUT 30 LABELS "portal;choosers;media"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QPA_PLATFORMTHEME=generic;DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent;DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent"
    ENVIRONMENT_MODIFICATION "DISPLAY=unset:;WAYLAND_DISPLAY=unset:")

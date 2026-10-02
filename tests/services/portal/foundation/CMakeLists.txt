# SPDX-License-Identifier: GPL-3.0-or-later
qt_add_executable(qindaqt_portal_access_tests tst_access.cpp)
target_link_libraries(qindaqt_portal_access_tests PRIVATE QindaQt::PortalFoundation Qt6::Test)
qindaqt_enable_warnings(qindaqt_portal_access_tests)
add_test(NAME qindaqt.portal-access COMMAND qindaqt_portal_access_tests)
set_tests_properties(qindaqt.portal-access PROPERTIES TIMEOUT 45 LABELS "portal;private-bus")

find_package(QindaTK CONFIG REQUIRED)
find_package(Wayland REQUIRED COMPONENTS Client)
set(consent_dir "${PROJECT_SOURCE_DIR}/src/services/portal/consent")
qt_add_executable(qindaqt_portal_consent_input "${consent_dir}/main.cpp"
    "${consent_dir}/consent_controller.cpp" "${consent_dir}/consent_controller.h" consent_input.cpp)
set_source_files_properties("${consent_dir}/Main.qml" PROPERTIES QT_RESOURCE_ALIAS Main.qml)
qt_add_qml_module(qindaqt_portal_consent_input URI QindaQt.PortalConsent VERSION 1.0
    RESOURCE_PREFIX /qt/qml OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/consent-qml"
    QML_FILES "${consent_dir}/Main.qml")
target_link_libraries(qindaqt_portal_consent_input PRIVATE QindaQt::PortalFoundation QindaQt::ForeignParent QindaQt::CompositorNames
    QindaTK::qindatk Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2 Qt6::Test Wayland::Client)
qindaqt_enable_warnings(qindaqt_portal_consent_input)
qt_add_executable(qindaqt_portal_native_consent_tests tst_native_consent.cpp)
target_link_libraries(qindaqt_portal_native_consent_tests PRIVATE QindaQt::PortalFoundation QindaQt::PortalEmail QindaQt::CompositorNames Qt6::Test)
qindaqt_enable_warnings(qindaqt_portal_native_consent_tests)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME qindaqt.portal-native-consent COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/run_native_consent.py" "$<TARGET_FILE:qindaqt_portal_native_consent_tests>"
    "$<TARGET_FILE:qindaqt_portal_consent_input>" "${QINDAQT_KWIN_WAYLAND}"
    "$<TARGET_FILE:qindaqt-uri-relay>" "$<TARGET_FILE:qindaqt_portal_mail_draft>")
set_tests_properties(qindaqt.portal-native-consent PROPERTIES TIMEOUT 150
    SKIP_RETURN_CODE 77 LABELS "portal;private-wayland")

qt_add_executable(qindaqt_portal_notification_tests tst_notifications.cpp)
target_link_libraries(qindaqt_portal_notification_tests PRIVATE QindaQt::PortalNotifications QindaQt::NotificationHost Qt6::Test Qt6::Gui)
qindaqt_enable_warnings(qindaqt_portal_notification_tests)
add_test(NAME qindaqt.portal-notifications COMMAND qindaqt_portal_notification_tests)
set_tests_properties(qindaqt.portal-notifications PROPERTIES TIMEOUT 45 LABELS "portal;native-notifications;private-bus")

find_package(KF6Config 6.0 REQUIRED CONFIG)
qt_add_executable(qindaqt_portal_email_tests tst_email.cpp)
target_link_libraries(qindaqt_portal_email_tests PRIVATE QindaQt::PortalEmail Qt6::Test)
qindaqt_enable_warnings(qindaqt_portal_email_tests)
add_test(NAME qindaqt.portal-email COMMAND qindaqt_portal_email_tests)
qt_add_executable(qindaqt_portal_inhibit_tests tst_inhibit.cpp)
target_link_libraries(qindaqt_portal_inhibit_tests PRIVATE QindaQt::PortalInhibit QindaQt::PowerService Qt6::Test)
qindaqt_enable_warnings(qindaqt_portal_inhibit_tests)
add_test(NAME qindaqt.portal-inhibit COMMAND qindaqt_portal_inhibit_tests)
set_tests_properties(qindaqt.portal-email qindaqt.portal-inhibit PROPERTIES TIMEOUT 45 LABELS "portal;private-bus")

qt_add_executable(qindaqt_portal_mail_draft mail_draft.cpp)
target_link_libraries(qindaqt_portal_mail_draft PRIVATE Qt6::Core Qt6::Gui Qt6::Quick Wayland::Client)
qindaqt_enable_warnings(qindaqt_portal_mail_draft)

find_package(Qt6 6.11 REQUIRED COMPONENTS GuiPrivate WaylandClient WaylandScannerTools)
enable_language(C)
qt_add_executable(qindaqt_portal_foreign_exporter foreign_exporter.cpp)
find_file(PORTAL_FOREIGN_TEST_XML xdg-foreign-unstable-v2.xml PATHS /usr/share/wayland-protocols/unstable/xdg-foreign REQUIRED)
qt6_generate_wayland_protocol_client_sources(qindaqt_portal_foreign_exporter FILES "${PORTAL_FOREIGN_TEST_XML}")
set_source_files_properties("${CMAKE_CURRENT_BINARY_DIR}/qwayland-xdg-foreign-unstable-v2.cpp" PROPERTIES COMPILE_OPTIONS "-w")
target_include_directories(qindaqt_portal_foreign_exporter SYSTEM PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(qindaqt_portal_foreign_exporter PRIVATE Qt6::Gui Qt6::Quick Qt6::GuiPrivate Qt6::WaylandClient Wayland::Client)
qindaqt_enable_warnings(qindaqt_portal_foreign_exporter)
target_compile_definitions(qindaqt_portal_native_consent_tests PRIVATE QINDAQT_PORTAL_FOREIGN_EXPORTER="$<TARGET_FILE:qindaqt_portal_foreign_exporter>")
add_dependencies(qindaqt_portal_native_consent_tests qindaqt_portal_foreign_exporter)

# AGENT-GUARD: this real frontend fixture uses production supervisor support.
# Bridge-only configurations intentionally omit its production support target.
if(TARGET QindaQt::SessionSupervisorSupport)
    qt_add_executable(qindaqt_portal_native_frontend_tests tst_native_frontend.cpp)
    target_include_directories(qindaqt_portal_native_frontend_tests PRIVATE "${PROJECT_SOURCE_DIR}")
    target_link_libraries(qindaqt_portal_native_frontend_tests PRIVATE QindaQt::PortalService
        QindaQt::PortalComposition QindaQt::SecretPortal QindaQt::NotificationHost
        QindaQt::SessionSupervisorSupport Qt6::Test)
    target_compile_definitions(qindaqt_portal_native_frontend_tests PRIVATE
        QINDAQT_FRONTEND_EXECUTABLE="${QINDAQT_XDG_DESKTOP_PORTAL_EXECUTABLE}")
    qindaqt_enable_warnings(qindaqt_portal_native_frontend_tests)
    add_test(NAME qindaqt.portal-native-frontend COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/run_native_frontend.py" "$<TARGET_FILE:qindaqt_portal_native_frontend_tests>"
        "$<TARGET_FILE:qindaqt_portal_consent_input>" "${QINDAQT_KWIN_WAYLAND}"
        "$<TARGET_FILE:qindaqt-uri-relay>" "$<TARGET_FILE:qindaqt_portal_mail_draft>"
        "${PROJECT_SOURCE_DIR}/src/services/portal/data/qindaqt.portal"
        "${PROJECT_SOURCE_DIR}/src/services/portal/data/qindaqt-portals.conf")
    set_tests_properties(qindaqt.portal-native-frontend PROPERTIES TIMEOUT 150
        SKIP_RETURN_CODE 77 RUN_SERIAL TRUE LABELS "portal;frontend;private-wayland")

    add_dependencies(qindaqt_portal_native_frontend_tests qindaqt_portal_consent_input
        qindaqt_portal_mail_draft qindaqt-uri-relay)
endif()

# Focused builds must produce every executable named by these private runners,
# rather than turning a missing input helper into a false admission regression.
add_dependencies(qindaqt_portal_native_consent_tests qindaqt_portal_consent_input
    qindaqt_portal_mail_draft qindaqt-uri-relay)

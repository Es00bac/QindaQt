# SPDX-License-Identifier: GPL-3.0-or-later
find_program(DBUS_RUN_SESSION dbus-run-session REQUIRED)
qt_add_executable(qindaqt_portal_permissions_store_test tst_permission_store.cpp)
target_link_libraries(qindaqt_portal_permissions_store_test PRIVATE qindaqt_settings_portal_permissions Qt6::Test)
qindaqt_enable_warnings(qindaqt_portal_permissions_store_test)
add_test(NAME qindaqt.settings-portal-permissions-store
    COMMAND "${DBUS_RUN_SESSION}" --config-file=${CMAKE_CURRENT_SOURCE_DIR}/private-bus.conf -- $<TARGET_FILE:qindaqt_portal_permissions_store_test>)
set_tests_properties(qindaqt.settings-portal-permissions-store PROPERTIES
    TIMEOUT 20 ENVIRONMENT "QT_FATAL_WARNINGS=1" LABELS "settings;portal;private-bus")
qt_add_executable(qindaqt_portal_permissions_page_test tst_permissions_page.cpp)
target_compile_definitions(qindaqt_portal_permissions_page_test PRIVATE
    QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}" QINDAQT_QML_IMPORT_PATH="${CMAKE_BINARY_DIR}/qml")
target_link_libraries(qindaqt_portal_permissions_page_test PRIVATE
    qindaqt_settings_appearance QindaQt::TokensQml qindaqt_themes Qt6::Quick Qt6::QuickControls2 Qt6::Test)
add_dependencies(qindaqt_portal_permissions_page_test qindaqt_settings_portal_permissions_qmlplugin
    qindaqt_tokens_qmlplugin qindaqt_controls_qmlplugin)
qindaqt_enable_warnings(qindaqt_portal_permissions_page_test)
add_test(NAME qindaqt.settings-portal-permissions-page COMMAND qindaqt_portal_permissions_page_test)
set_tests_properties(qindaqt.settings-portal-permissions-page PROPERTIES TIMEOUT 20
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_FATAL_WARNINGS=1;DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent;DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent"
    LABELS "settings;portal;offscreen")
# Actual public executable route + active Loader witness, on a disposable bus.
add_test(NAME qindaqt.settings-portal-permissions-route
    COMMAND "${DBUS_RUN_SESSION}" --config-file=${CMAKE_CURRENT_SOURCE_DIR}/private-bus.conf -- $<TARGET_FILE:qindaqt-settings>
    --page portal-permissions --route-construction-probe)
set_tests_properties(qindaqt.settings-portal-permissions-route PROPERTIES TIMEOUT 20
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_FATAL_WARNINGS=1;DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent"
    LABELS "settings;portal;route")

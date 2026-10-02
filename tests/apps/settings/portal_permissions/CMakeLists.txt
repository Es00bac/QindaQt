# SPDX-License-Identifier: GPL-3.0-or-later
find_program(DBUS_RUN_SESSION dbus-run-session REQUIRED)
qt_add_executable(qindaqt_portal_permissions_store_test tst_permission_store.cpp)
target_link_libraries(qindaqt_portal_permissions_store_test PRIVATE qindaqt_settings_portal_permissions Qt6::Test)
qindaqt_enable_warnings(qindaqt_portal_permissions_store_test)
add_test(NAME qindaqt.settings-portal-permissions-store
    COMMAND "${DBUS_RUN_SESSION}" -- $<TARGET_FILE:qindaqt_portal_permissions_store_test>)
set_tests_properties(qindaqt.settings-portal-permissions-store PROPERTIES
    TIMEOUT 20 ENVIRONMENT "QT_FATAL_WARNINGS=1" LABELS "settings;portal;private-bus")

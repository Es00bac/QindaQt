# SPDX-License-Identifier: LGPL-3.0-or-later
qt_add_executable(qindaqt_media_inventory_tests tst_media_inventory.cpp)
target_link_libraries(qindaqt_media_inventory_tests PRIVATE
    QindaQt::RemovableMediaClient qindaqt_removable_media_core Qt6::Test)
qindaqt_enable_warnings(qindaqt_media_inventory_tests)
find_program(QINDAQT_MEDIA_CLIENT_DBUS_SESSION dbus-run-session REQUIRED)
add_test(NAME qindaqt.removable-media-inventory
    COMMAND "${QINDAQT_MEDIA_CLIENT_DBUS_SESSION}" -- $<TARGET_FILE:qindaqt_media_inventory_tests>)
set_tests_properties(qindaqt.removable-media-inventory PROPERTIES
    ENVIRONMENT "DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent"
    LABELS "media;client;integration;private-bus" TIMEOUT 60)

qt_add_executable(qindaqt_media_actions_tests tst_media_actions.cpp)
target_link_libraries(qindaqt_media_actions_tests PRIVATE
    QindaQt::RemovableMediaClient qindaqt_removable_media_core Qt6::Test)
qindaqt_enable_warnings(qindaqt_media_actions_tests)
add_test(NAME qindaqt.removable-media-actions
    COMMAND "${QINDAQT_MEDIA_CLIENT_DBUS_SESSION}" -- $<TARGET_FILE:qindaqt_media_actions_tests>)
set_tests_properties(qindaqt.removable-media-actions PROPERTIES
    ENVIRONMENT "DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent"
    LABELS "media;client;actions;integration;private-bus" TIMEOUT 60)

if(TARGET qindaqt_removable_media_launch)
    qt_add_executable(qindaqt_media_owner_launcher_tests tst_owner_launcher.cpp)
    target_link_libraries(qindaqt_media_owner_launcher_tests PRIVATE QindaQt::RemovableMediaLaunch Qt6::Test)
    qindaqt_enable_warnings(qindaqt_media_owner_launcher_tests)
    add_test(NAME qindaqt.removable-media-owner-launcher COMMAND qindaqt_media_owner_launcher_tests)
endif()

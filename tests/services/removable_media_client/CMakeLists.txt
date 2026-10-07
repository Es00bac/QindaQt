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

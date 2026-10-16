# SPDX-License-Identifier: GPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Test)
# Imported targets are directory-scoped: the module-load row links libobs itself.
find_package(libobs REQUIRED)

qt_add_executable(qindaqt_obs_bridge_sync_tests tst_console_sources.cpp support.h)
target_link_libraries(qindaqt_obs_bridge_sync_tests PRIVATE QindaQt::ObsBridgeCore Qt6::Test)
qindaqt_enable_warnings(qindaqt_obs_bridge_sync_tests)
add_test(NAME qindaqt.obs-bridge-sync COMMAND qindaqt_obs_bridge_sync_tests)
set_tests_properties(qindaqt.obs-bridge-sync PROPERTIES TIMEOUT 20 LABELS "obs;audio")

qt_add_executable(qindaqt_obs_bridge_libobs_tests tst_obs_bridge_libobs.cpp support.h)
target_link_libraries(
    qindaqt_obs_bridge_libobs_tests
    PRIVATE QindaQt::ObsBridgeModule Qt6::Test
)
qindaqt_enable_warnings(qindaqt_obs_bridge_libobs_tests)
add_test(NAME qindaqt.obs-bridge-libobs COMMAND qindaqt_obs_bridge_libobs_tests)
set_tests_properties(
    qindaqt.obs-bridge-libobs
    PROPERTIES TIMEOUT 60 LABELS "obs;audio" ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

qt_add_executable(qindaqt_obs_bridge_module_load_tests tst_obs_module_load.cpp)
target_link_libraries(qindaqt_obs_bridge_module_load_tests PRIVATE OBS::libobs Qt6::Test)
target_compile_definitions(
    qindaqt_obs_bridge_module_load_tests
    PRIVATE QINDAQT_OBS_MODULE_PATH="$<TARGET_FILE:obs-qindaqt>"
)
add_dependencies(qindaqt_obs_bridge_module_load_tests obs-qindaqt)
qindaqt_enable_warnings(qindaqt_obs_bridge_module_load_tests)
add_test(NAME qindaqt.obs-bridge-module-load COMMAND qindaqt_obs_bridge_module_load_tests)
set_tests_properties(
    qindaqt.obs-bridge-module-load
    PROPERTIES TIMEOUT 60 LABELS "obs;audio" ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

# ADR-0394: the console dock holds an Audio1 meter lease exactly while it is
# visible. DockMeterLease is driven over the real AudioClient on the shared fake
# transport, with no OBS frontend, no libobs startup and no bus; the session bus
# is pointed at nothing anyway so no row can activate a real Audio1 on the host.
find_package(Qt6 6.11 REQUIRED COMPONENTS Widgets)
qt_add_executable(qindaqt_obs_console_dock_tests tst_obs_console_dock.cpp)
target_include_directories(
    qindaqt_obs_console_dock_tests
    PRIVATE "${PROJECT_SOURCE_DIR}/tests/services/audio_client"
)
target_link_libraries(
    qindaqt_obs_console_dock_tests
    PRIVATE QindaQt::ObsBridgeModule Qt6::Widgets Qt6::Test
)
qindaqt_enable_warnings(qindaqt_obs_console_dock_tests)
add_test(NAME qindaqt.obs-console-dock COMMAND qindaqt_obs_console_dock_tests)
set_tests_properties(
    qindaqt.obs-console-dock
    PROPERTIES
        TIMEOUT 60
        LABELS "obs;audio"
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent/qindaqt-test-bus"
)

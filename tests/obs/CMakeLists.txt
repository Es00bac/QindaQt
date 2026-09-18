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

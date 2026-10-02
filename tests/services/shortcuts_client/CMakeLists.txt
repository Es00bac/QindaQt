# SPDX-License-Identifier: GPL-3.0-or-later
# Optional exact cross-repository fixture. Uses already-built isolated authority
# archives; no production service seam, install or ambient session dependency.
set(QINDAQT_SHORTCUT_AUTHORITY_TEST_ROOT "" CACHE PATH "Exact isolated qindaqt-kwin source fixture for native shortcut cross-repo tests")
if(QINDAQT_SHORTCUT_AUTHORITY_TEST_ROOT)
    find_package(KF6Config REQUIRED)
    find_package(KF6KIO REQUIRED)
    find_package(KF6Service REQUIRED)
    foreach(lib IN ITEMS qindaqt_kwin_shortcuts qindaqt_kwin_shortcut_endpoint)
        add_library(native_test_${lib} STATIC IMPORTED)
        set_target_properties(native_test_${lib} PROPERTIES IMPORTED_LOCATION "${QINDAQT_SHORTCUT_AUTHORITY_TEST_ROOT}/build/shortcut-contracts/lib${lib}.a")
    endforeach()
    qt_add_executable(qindaqt_native_shortcut_bridge_tests tst_native_bridge.cpp)
    target_include_directories(qindaqt_native_shortcut_bridge_tests PRIVATE
        "${PROJECT_SOURCE_DIR}" "${QINDAQT_SHORTCUT_AUTHORITY_TEST_ROOT}" "${QINDAQT_SHORTCUT_AUTHORITY_TEST_ROOT}/src")
    target_compile_definitions(qindaqt_native_shortcut_bridge_tests PRIVATE QINDAQT_DBUS_DAEMON_EXECUTABLE="/usr/bin/dbus-daemon")
    target_link_libraries(qindaqt_native_shortcut_bridge_tests PRIVATE
        qindaqt_settings_input QindaQt::PortalShortcuts native_test_qindaqt_kwin_shortcut_endpoint native_test_qindaqt_kwin_shortcuts
        KF6::ConfigCore KF6::KIOGui KF6::Service Qt6::Test)
    qindaqt_enable_warnings(qindaqt_native_shortcut_bridge_tests)
    add_test(NAME qindaqt.native-shortcuts-cross-repository COMMAND qindaqt_native_shortcut_bridge_tests)
    set_tests_properties(qindaqt.native-shortcuts-cross-repository PROPERTIES LABELS "portal;settings;shortcuts;dbus" TIMEOUT 30 RUN_SERIAL TRUE)
endif()

# SPDX-License-Identifier: GPL-3.0-or-later

# QindaQt Screenshot (ADR-0289): policy, geometry, flow, OBS record state
# machine, preferences and the QindaTK window offscreen. No KWin, OBS or
# notification host is contacted; D-Bus points nowhere.
set(screenshot_test_root "${CMAKE_CURRENT_BINARY_DIR}/isolated")
file(MAKE_DIRECTORY "${screenshot_test_root}/home" "${screenshot_test_root}/runtime")
file(CHMOD "${screenshot_test_root}/runtime" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)
set(screenshot_test_env
    "QT_QPA_PLATFORM=offscreen;QSG_RHI_BACKEND=software;QT_QUICK_BACKEND=software;QT_QPA_PLATFORMTHEME=;QT_FATAL_WARNINGS=1;QML_IMPORT_PATH=${CMAKE_BINARY_DIR}/qml;HOME=${screenshot_test_root}/home;XDG_CONFIG_HOME=${screenshot_test_root}/home/config;XDG_DATA_HOME=${screenshot_test_root}/home/data;XDG_RUNTIME_DIR=${screenshot_test_root}/runtime;DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent;DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent;DISPLAY=;WAYLAND_DISPLAY=")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/artifacts")

function(qindaqt_add_screenshot_test name source)
    qt_add_executable(qindaqt_screenshot_${name}_tests ${source} ${ARGN})
    target_link_libraries(qindaqt_screenshot_${name}_tests PRIVATE qindaqt_screenshot_support Qt6::Test)
    target_include_directories(qindaqt_screenshot_${name}_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_compile_definitions(qindaqt_screenshot_${name}_tests PRIVATE
        QINDAQT_SETTINGS_SCHEMA="${PROJECT_SOURCE_DIR}/data/settings/schema-v2.json"
        SCREENSHOT_QML_DIR="${PROJECT_SOURCE_DIR}/src/apps/screenshot/qml"
        SCREENSHOT_ARTIFACTS_DIR="${CMAKE_CURRENT_BINARY_DIR}/artifacts")
    qindaqt_enable_warnings(qindaqt_screenshot_${name}_tests)
    add_test(NAME qindaqt.apps-screenshot-${name} COMMAND qindaqt_screenshot_${name}_tests)
    set_tests_properties(qindaqt.apps-screenshot-${name} PROPERTIES
        ENVIRONMENT "${screenshot_test_env}" LABELS "apps;screenshot" TIMEOUT 60)
endfunction()

qindaqt_add_screenshot_test(policy tst_capture_policy.cpp)
qindaqt_add_screenshot_test(geometry tst_capture_geometry.cpp)
qindaqt_add_screenshot_test(flow tst_capture_flow.cpp fake_capture_port.h)
qindaqt_add_screenshot_test(record tst_record_controller.cpp)
qindaqt_add_screenshot_test(preferences tst_screenshot_preferences.cpp)
target_link_libraries(qindaqt_screenshot_preferences_tests PRIVATE QindaQt::SettingsClient)
qindaqt_add_screenshot_test(ui tst_screenshot_ui.cpp fake_capture_port.h)
target_link_libraries(qindaqt_screenshot_ui_tests PRIVATE Qt6::Qml Qt6::Quick)

# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_executable(qindaqt_power_settings_model_tests
    tst_power_settings_model.cpp power_settings_test_support.h)
target_link_libraries(qindaqt_power_settings_model_tests
    PRIVATE qindaqt_settings_power Qt6::Test)
qindaqt_enable_warnings(qindaqt_power_settings_model_tests)
add_test(NAME qindaqt.settings-power-model
    COMMAND qindaqt_power_settings_model_tests)
set_tests_properties(qindaqt.settings-power-model PROPERTIES
    ENVIRONMENT "DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent"
    LABELS "settings;power;model")

qt_add_executable(qindaqt_screen_lock_settings_tests
    tst_screen_lock_settings.cpp)
target_link_libraries(qindaqt_screen_lock_settings_tests
    PRIVATE qindaqt_settings_power Qt6::Test)
qindaqt_enable_warnings(qindaqt_screen_lock_settings_tests)
add_test(NAME qindaqt.settings-screen-lock-model
    COMMAND qindaqt_screen_lock_settings_tests)
set_tests_properties(qindaqt.settings-screen-lock-model PROPERTIES
    LABELS "settings;power;screen-lock")

qt_add_executable(qindaqt_screen_lock_resume_settings_tests
    tst_screen_lock_resume_settings.cpp)
target_link_libraries(qindaqt_screen_lock_resume_settings_tests
    PRIVATE qindaqt_settings_power Qt6::Test)
qindaqt_enable_warnings(qindaqt_screen_lock_resume_settings_tests)
add_test(NAME qindaqt.settings-screen-lock-resume
    COMMAND qindaqt_screen_lock_resume_settings_tests)
set_tests_properties(qindaqt.settings-screen-lock-resume PROPERTIES
    LABELS "settings;power;screen-lock")

qt_add_executable(qindaqt_idle_display_settings_tests
    tst_idle_display_settings.cpp)
target_link_libraries(qindaqt_idle_display_settings_tests
    PRIVATE qindaqt_settings_power Qt6::Test)
qindaqt_enable_warnings(qindaqt_idle_display_settings_tests)
add_test(NAME qindaqt.settings-idle-display-model
    COMMAND qindaqt_idle_display_settings_tests)
set_tests_properties(qindaqt.settings-idle-display-model PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    LABELS "settings;power;idle-display")

qt_add_executable(qindaqt_power_settings_slider_tests
    tst_power_settings_slider.cpp power_settings_test_support.h)
target_link_libraries(qindaqt_power_settings_slider_tests
    PRIVATE qindaqt_settings_power Qt6::Test)
qindaqt_enable_warnings(qindaqt_power_settings_slider_tests)
add_test(NAME qindaqt.settings-power-slider-admission-debounce
    COMMAND qindaqt_power_settings_slider_tests)
set_tests_properties(qindaqt.settings-power-slider-admission-debounce PROPERTIES
    ENVIRONMENT "DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent"
    LABELS "settings;power;model;debounce")

qt_add_executable(qindaqt_power_page_tests
    tst_power_page.cpp stub_power_settings_model.h)
target_include_directories(qindaqt_power_page_tests PRIVATE "${PROJECT_SOURCE_DIR}")
target_compile_definitions(qindaqt_power_page_tests PRIVATE
    QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
    QINDAQT_QML_IMPORT_PATH="${CMAKE_BINARY_DIR}/qml"
    QINDAQT_POWER_PAGE_QML_PATH="${PROJECT_SOURCE_DIR}/src/apps/settings/power/qml/PowerPage.qml")
target_link_libraries(qindaqt_power_page_tests PRIVATE
    qindaqt_settings_power qindaqt_settings_appearance QindaQt::TokensQml
    qindaqt_themes Qt6::Quick Qt6::QuickControls2 Qt6::Test)
add_dependencies(qindaqt_power_page_tests qindaqt_tokens_qmlplugin
    qindaqt_controls_qmlplugin qindaqt_settings_power_qmlplugin)
qindaqt_enable_warnings(qindaqt_power_page_tests)
add_test(NAME qindaqt.settings-power-page COMMAND qindaqt_power_page_tests)
set_tests_properties(qindaqt.settings-power-page PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_LINUX_ACCESSIBILITY_ALWAYS_ON=1;QT_FATAL_WARNINGS=1;DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent"
    LABELS "offscreen;settings;power;accessibility")

add_test(NAME qindaqt.settings-power-boundary
    COMMAND "${CMAKE_COMMAND}" -DSOURCE_ROOT=${PROJECT_SOURCE_DIR}
    -P "${CMAKE_CURRENT_SOURCE_DIR}/check_boundary.cmake")
set_tests_properties(qindaqt.settings-power-boundary PROPERTIES
    LABELS "settings;power;boundary")

add_test(NAME qindaqt.settings-power-boundary-poison
    COMMAND "${CMAKE_COMMAND}"
    -DPOISON_ROOT=${CMAKE_CURRENT_BINARY_DIR}/boundary-poison
    -DCHECK_SCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/check_boundary.cmake
    -P "${CMAKE_CURRENT_SOURCE_DIR}/check_boundary_negative.cmake")
set_tests_properties(qindaqt.settings-power-boundary-poison PROPERTIES
    LABELS "settings;power;boundary;poison")

add_test(NAME qindaqt.settings-power-installed-route
    COMMAND "${CMAKE_COMMAND}"
    -DBUILD_DIRECTORY=${CMAKE_BINARY_DIR}
    -DINSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/installed-power-stage
    -DINSTALL_BINDIR=${CMAKE_INSTALL_BINDIR}
    -DINSTALL_DATADIR=${CMAKE_INSTALL_DATADIR}
    -DINSTALL_QMLDIR=${QT6_INSTALL_QML}
    -DSETTINGS_EXECUTABLE_NAME=qindaqt-settings${CMAKE_EXECUTABLE_SUFFIX}
    -DCONFIGURATION=$<CONFIG>
    -P "${CMAKE_CURRENT_SOURCE_DIR}/check_installed_route.cmake")
set_tests_properties(qindaqt.settings-power-installed-route PROPERTIES
    LABELS "offscreen;settings;power;package" RUN_SERIAL TRUE TIMEOUT 45)

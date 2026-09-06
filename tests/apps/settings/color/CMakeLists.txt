# SPDX-License-Identifier: GPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS WaylandClient)
find_package(Wayland REQUIRED COMPONENTS Client)

qt_add_executable(qindaqt_color_settings_model_tests
    tst_color_settings_model.cpp color_settings_test_support.h
    ${PROJECT_SOURCE_DIR}/tests/services/display_client/support/fake_display_transport.h)
target_include_directories(qindaqt_color_settings_model_tests
    PRIVATE "${PROJECT_SOURCE_DIR}")
target_link_libraries(qindaqt_color_settings_model_tests
    PRIVATE qindaqt_settings_color Qt6::Test)
qindaqt_enable_warnings(qindaqt_color_settings_model_tests)
add_test(NAME qindaqt.settings-color-model
    COMMAND qindaqt_color_settings_model_tests)
set_tests_properties(qindaqt.settings-color-model PROPERTIES
    ENVIRONMENT "DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent;DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent"
    LABELS "settings;color;model")

qt_add_executable(qindaqt_color_settings_apply_tests
    tst_color_settings_apply.cpp color_settings_test_support.h
    ${PROJECT_SOURCE_DIR}/tests/services/display_client/support/fake_display_transport.h)
target_include_directories(qindaqt_color_settings_apply_tests
    PRIVATE "${PROJECT_SOURCE_DIR}")
target_link_libraries(qindaqt_color_settings_apply_tests
    PRIVATE qindaqt_settings_color Qt6::Test)
qindaqt_enable_warnings(qindaqt_color_settings_apply_tests)
add_test(NAME qindaqt.settings-color-apply
    COMMAND qindaqt_color_settings_apply_tests)
set_tests_properties(qindaqt.settings-color-apply PROPERTIES
    ENVIRONMENT "DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent;DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent"
    LABELS "settings;color;model;apply")

qt_add_executable(qindaqt_color_route_composition_tests
    tst_color_route_composition.cpp color_settings_test_support.h)
target_include_directories(qindaqt_color_route_composition_tests
    PRIVATE "${PROJECT_SOURCE_DIR}")
target_link_libraries(qindaqt_color_route_composition_tests
    PRIVATE qindaqt_settings_color_backend_qml qindaqt_settings_color
    Qt6::Qml Qt6::Test)
qindaqt_enable_warnings(qindaqt_color_route_composition_tests)
add_test(NAME qindaqt.settings-color-composition
    COMMAND qindaqt_color_route_composition_tests)
set_tests_properties(qindaqt.settings-color-composition PROPERTIES
    ENVIRONMENT "DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent;DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent"
    LABELS "settings;color;composition")

qt_add_executable(qindaqt_color_page_tests
    tst_color_page.cpp stub_color_settings_model.h)
target_include_directories(qindaqt_color_page_tests PRIVATE "${PROJECT_SOURCE_DIR}")
target_compile_definitions(qindaqt_color_page_tests PRIVATE
    QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
    QINDAQT_QML_IMPORT_PATH="${CMAKE_BINARY_DIR}/qml"
    QINDAQT_COLOR_PAGE_QML_PATH="${PROJECT_SOURCE_DIR}/src/apps/settings/color/qml/ColorPage.qml")
target_link_libraries(qindaqt_color_page_tests PRIVATE
    qindaqt_settings_color qindaqt_settings_appearance QindaQt::TokensQml
    qindaqt_themes Qt6::Quick Qt6::QuickControls2 Qt6::Test)
add_dependencies(qindaqt_color_page_tests qindaqt_tokens_qmlplugin
    qindaqt_controls_qmlplugin qindaqt_settings_color_qmlplugin)
qindaqt_enable_warnings(qindaqt_color_page_tests)
add_test(NAME qindaqt.settings-color-page COMMAND qindaqt_color_page_tests)
set_tests_properties(qindaqt.settings-color-page PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_LINUX_ACCESSIBILITY_ALWAYS_ON=1;QT_FATAL_WARNINGS=1;DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent;DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent"
    LABELS "offscreen;settings;color;accessibility")

qt_add_executable(qindaqt_color_navigation_page_tests
    tst_color_navigation_page.cpp
    stub_color_settings_model.h color_navigation_assertions.h)
target_include_directories(qindaqt_color_navigation_page_tests
    PRIVATE "${PROJECT_SOURCE_DIR}")
target_compile_definitions(qindaqt_color_navigation_page_tests PRIVATE
    QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
    QINDAQT_SETTINGS_SOURCE_DIR="${PROJECT_SOURCE_DIR}/src/apps/settings_center"
    QINDAQT_QML_IMPORT_PATH="${CMAKE_BINARY_DIR}/qml")
target_link_libraries(
    qindaqt_color_navigation_page_tests PRIVATE
    QindaQt::SettingsNavigation qindaqt_settings_appearance
    qindaqt_settings_customize_qml qindaqt_settings_customize_qmlplugin
    # AGENT-NOTE: Main.qml imports every route module; the Power, Clipboard,
    # and Color modules are static, so this in-process Main.qml host links them.
    qindaqt_settings_power qindaqt_settings_power_backend_qml
    qindaqt_settings_power_backend_qmlplugin
    qindaqt_settings_clipboard_qml qindaqt_settings_clipboard_qmlplugin
    qindaqt_settings_color_backend_qml
    qindaqt_settings_color_backend_qmlplugin
    qindaqt_design_tokens qindaqt_tokens_qml qindaqt_controls_qml
    qindaqt_themes Qt6::Quick Qt6::QuickControls2 Qt6::Test
)
add_dependencies(
    qindaqt_color_navigation_page_tests
    qindaqt_settings_appearance_qmlplugin qindaqt_settings_display_qmlplugin
    qindaqt_settings_network_qmlplugin qindaqt_settings_customize_qmlplugin
    qindaqt_settings_audio_qmlplugin qindaqt_settings_bluetooth_qmlplugin
    qindaqt_settings_power_qmlplugin qindaqt_settings_clipboard_qmlplugin
    qindaqt_settings_color_qmlplugin
    qindaqt_controls_qmlplugin qindaqt_tokens_qmlplugin
)
qindaqt_enable_warnings(qindaqt_color_navigation_page_tests)
add_test(NAME qindaqt.settings-color-navigation-page
    COMMAND qindaqt_color_navigation_page_tests)
set_tests_properties(qindaqt.settings-color-navigation-page PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_LINUX_ACCESSIBILITY_ALWAYS_ON=1;QT_FATAL_WARNINGS=1;DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent;DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent"
    LABELS "offscreen;settings;color;navigation")

add_test(NAME qindaqt.settings-color-boundary
    COMMAND "${CMAKE_COMMAND}" -DSOURCE_ROOT=${PROJECT_SOURCE_DIR}
    -P "${CMAKE_CURRENT_SOURCE_DIR}/check_boundary.cmake")
set_tests_properties(qindaqt.settings-color-boundary PROPERTIES
    LABELS "settings;color;boundary")

add_test(NAME qindaqt.settings-color-boundary-poison
    COMMAND "${CMAKE_COMMAND}"
    -DPOISON_ROOT=${CMAKE_CURRENT_BINARY_DIR}/boundary-poison
    -DCHECK_SCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/check_boundary.cmake
    -P "${CMAKE_CURRENT_SOURCE_DIR}/check_boundary_negative.cmake")
set_tests_properties(qindaqt.settings-color-boundary-poison PROPERTIES
    LABELS "settings;color;boundary;poison")

add_test(NAME qindaqt.settings-color-installed-route
    COMMAND "${CMAKE_COMMAND}"
    -DBUILD_DIRECTORY=${CMAKE_BINARY_DIR}
    -DINSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/installed-color-stage
    -DINSTALL_BINDIR=${CMAKE_INSTALL_BINDIR}
    -DINSTALL_DATADIR=${CMAKE_INSTALL_DATADIR}
    -DINSTALL_QMLDIR=${QT6_INSTALL_QML}
    -DSETTINGS_EXECUTABLE_NAME=qindaqt-settings${CMAKE_EXECUTABLE_SUFFIX}
    -DCONFIGURATION=$<CONFIG>
    -P "${CMAKE_CURRENT_SOURCE_DIR}/check_installed_route.cmake")
set_tests_properties(qindaqt.settings-color-installed-route PROPERTIES
    LABELS "offscreen;settings;color;package" RUN_SERIAL TRUE TIMEOUT 45)

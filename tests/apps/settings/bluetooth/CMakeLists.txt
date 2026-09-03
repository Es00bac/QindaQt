# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_executable(
    qindaqt_bluetooth_settings_model_tests
    tst_bluetooth_settings_model.cpp
    bluetooth_settings_test_support.h
)
target_link_libraries(
    qindaqt_bluetooth_settings_model_tests
    PRIVATE qindaqt_settings_bluetooth Qt6::Test
)
qindaqt_enable_warnings(qindaqt_bluetooth_settings_model_tests)
add_test(
    NAME qindaqt.settings-bluetooth-model
    COMMAND qindaqt_bluetooth_settings_model_tests
)
set_tests_properties(
    qindaqt.settings-bluetooth-model PROPERTIES
    ENVIRONMENT "DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent"
    LABELS "settings;bluetooth;model"
)

qt_add_executable(
    qindaqt_bluetooth_settings_adversarial_tests
    tst_bluetooth_settings_adversarial.cpp
    bluetooth_settings_test_support.h
)
target_link_libraries(
    qindaqt_bluetooth_settings_adversarial_tests
    PRIVATE qindaqt_settings_bluetooth Qt6::Test
)
qindaqt_enable_warnings(qindaqt_bluetooth_settings_adversarial_tests)
add_test(
    NAME qindaqt.settings-bluetooth-model-adversarial
    COMMAND qindaqt_bluetooth_settings_adversarial_tests
)
set_tests_properties(
    qindaqt.settings-bluetooth-model-adversarial PROPERTIES
    ENVIRONMENT "DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent"
    LABELS "settings;bluetooth;model;adversarial;lifetime"
)

qt_add_executable(
    qindaqt_bluetooth_page_tests
    tst_bluetooth_page.cpp
    stub_bluetooth_settings_model.h
)
target_include_directories(
    qindaqt_bluetooth_page_tests PRIVATE "${PROJECT_SOURCE_DIR}"
)
target_compile_definitions(
    qindaqt_bluetooth_page_tests PRIVATE
    QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
    QINDAQT_QML_IMPORT_PATH="${CMAKE_BINARY_DIR}/qml"
    QINDAQT_BLUETOOTH_PAGE_QML_PATH="${PROJECT_SOURCE_DIR}/src/apps/settings/bluetooth/qml/BluetoothPage.qml"
)
target_link_libraries(
    qindaqt_bluetooth_page_tests
    PRIVATE qindaqt_settings_bluetooth qindaqt_settings_appearance
            QindaQt::TokensQml qindaqt_themes Qt6::Quick
            Qt6::QuickControls2 Qt6::Test
)
add_dependencies(
    qindaqt_bluetooth_page_tests
    qindaqt_tokens_qmlplugin qindaqt_controls_qmlplugin
    qindaqt_settings_bluetooth_qmlplugin
)
qindaqt_enable_warnings(qindaqt_bluetooth_page_tests)
add_test(NAME qindaqt.settings-bluetooth-page COMMAND qindaqt_bluetooth_page_tests)
set_tests_properties(
    qindaqt.settings-bluetooth-page PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_LINUX_ACCESSIBILITY_ALWAYS_ON=1;QT_FATAL_WARNINGS=1;DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent"
    LABELS "offscreen;settings;bluetooth;accessibility"
)

qt_add_executable(
    qindaqt_bluetooth_window_close_tests
    tst_bluetooth_window_close.cpp
    stub_bluetooth_settings_model.h
)
target_include_directories(
    qindaqt_bluetooth_window_close_tests PRIVATE "${PROJECT_SOURCE_DIR}"
)
target_compile_definitions(
    qindaqt_bluetooth_window_close_tests PRIVATE
    QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
    QINDAQT_SETTINGS_SOURCE_DIR="${PROJECT_SOURCE_DIR}/src/apps/settings_center"
    QINDAQT_QML_IMPORT_PATH="${CMAKE_BINARY_DIR}/qml"
)
target_link_libraries(
    qindaqt_bluetooth_window_close_tests PRIVATE
    QindaQt::SettingsNavigation qindaqt_settings_appearance
    qindaqt_settings_appearance_qml qindaqt_settings_display_qml
    qindaqt_settings_network_qml qindaqt_settings_customize_qml
    qindaqt_settings_customize_qmlplugin qindaqt_settings_bluetooth
    qindaqt_settings_bluetooth_qml
    qindaqt_design_tokens qindaqt_tokens_qml qindaqt_controls_qml
    qindaqt_themes Qt6::Quick Qt6::Test
)
add_dependencies(
    qindaqt_bluetooth_window_close_tests qindaqt-settings
    qindaqt_settings_appearance_qmlplugin qindaqt_settings_display_qmlplugin
    qindaqt_settings_network_qmlplugin
    qindaqt_settings_bluetooth_qmlplugin qindaqt_settings_customize_qmlplugin
    qindaqt_controls_qmlplugin qindaqt_tokens_qmlplugin
)
qindaqt_enable_warnings(qindaqt_bluetooth_window_close_tests)
add_test(
    NAME qindaqt.settings-bluetooth-window-close
    COMMAND qindaqt_bluetooth_window_close_tests
)
set_tests_properties(
    qindaqt.settings-bluetooth-window-close PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_FATAL_WARNINGS=1;DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent"
    LABELS "offscreen;settings;bluetooth;lifetime"
)

add_test(
    NAME qindaqt.settings-bluetooth-boundary
    COMMAND "${CMAKE_COMMAND}" -DSOURCE_ROOT=${PROJECT_SOURCE_DIR}
            -P "${CMAKE_CURRENT_SOURCE_DIR}/check_boundary.cmake"
)
set_tests_properties(qindaqt.settings-bluetooth-boundary PROPERTIES
    LABELS "settings;bluetooth;boundary")

add_test(
    NAME qindaqt.settings-bluetooth-boundary-poison
    COMMAND "${CMAKE_COMMAND}"
            -DPOISON_ROOT=${CMAKE_CURRENT_BINARY_DIR}/boundary-poison
            -DCHECK_SCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/check_boundary.cmake
            -P "${CMAKE_CURRENT_SOURCE_DIR}/check_boundary_negative.cmake"
)
set_tests_properties(qindaqt.settings-bluetooth-boundary-poison PROPERTIES
    LABELS "settings;bluetooth;boundary;poison")

add_test(
    NAME qindaqt.settings-bluetooth-installed-route
    COMMAND "${CMAKE_COMMAND}"
            -DBUILD_DIRECTORY=${CMAKE_BINARY_DIR}
            -DINSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/installed-bluetooth-stage
            -DINSTALL_BINDIR=${CMAKE_INSTALL_BINDIR}
            -DINSTALL_DATADIR=${CMAKE_INSTALL_DATADIR}
            -DINSTALL_QMLDIR=${QT6_INSTALL_QML}
            -DSETTINGS_EXECUTABLE_NAME=qindaqt-settings${CMAKE_EXECUTABLE_SUFFIX}
            -DCONFIGURATION=$<CONFIG>
            -P "${CMAKE_CURRENT_SOURCE_DIR}/check_installed_route.cmake"
)
set_tests_properties(
    qindaqt.settings-bluetooth-installed-route PROPERTIES
    LABELS "offscreen;settings;bluetooth;package" RUN_SERIAL TRUE TIMEOUT 45
)

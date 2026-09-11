# SPDX-License-Identifier: GPL-3.0-or-later

find_program(QINDAQT_DBUS_DAEMON_EXECUTABLE NAMES dbus-daemon REQUIRED)

function(qindaqt_input_test target)
    cmake_parse_arguments(ARG "" "" "SOURCES" ${ARGN})
    qt_add_executable(${target} ${ARG_SOURCES})
    target_include_directories(${target} PRIVATE ${PROJECT_SOURCE_DIR})
    target_compile_definitions(
        ${target}
        PRIVATE QINDAQT_DBUS_DAEMON_EXECUTABLE="${QINDAQT_DBUS_DAEMON_EXECUTABLE}"
    )
    target_link_libraries(${target} PRIVATE qindaqt_settings_input Qt6::Core
                                            Qt6::DBus Qt6::Gui Qt6::Test)
    qindaqt_enable_warnings(${target})
endfunction()

# Pointer/touchpad authority port over a private bus with a fake KWin.
qindaqt_input_test(
    qindaqt_settings_input_pointer_port_test
    SOURCES
        tst_pointer_device_port.cpp
        support/fake_kwin_input.h
        support/private_bus.h
)
add_test(NAME qindaqt.settings-input-pointer-port
         COMMAND qindaqt_settings_input_pointer_port_test)
set_tests_properties(
    qindaqt.settings-input-pointer-port PROPERTIES
    LABELS "settings;input;dbus" TIMEOUT 60 RUN_SERIAL TRUE
)

# Keyboard repeat/NumLock config port: KConfig round trips in a temp dir.
qindaqt_input_test(
    qindaqt_settings_input_keyboard_config_port_test
    SOURCES
        tst_keyboard_config_port.cpp
        support/private_bus.h
)
add_test(NAME qindaqt.settings-input-keyboard-config-port
         COMMAND qindaqt_settings_input_keyboard_config_port_test)
set_tests_properties(
    qindaqt.settings-input-keyboard-config-port PROPERTIES
    LABELS "settings;input;dbus" TIMEOUT 60 RUN_SERIAL TRUE
)

# Layout port (kxkbrc) plus the bounded evdev catalog parser.
qindaqt_input_test(
    qindaqt_settings_input_keyboard_layout_port_test
    SOURCES
        tst_keyboard_layout_port.cpp
)
add_test(NAME qindaqt.settings-input-keyboard-layout-port
         COMMAND qindaqt_settings_input_keyboard_layout_port_test)
set_tests_properties(
    qindaqt.settings-input-keyboard-layout-port PROPERTIES
    LABELS "settings;input" TIMEOUT 60
)

# Shortcut authority port over a private bus with a fake kglobalaccel.
qindaqt_input_test(
    qindaqt_settings_input_shortcut_port_test
    SOURCES
        tst_shortcut_port.cpp
        support/fake_kglobalaccel.h
        support/private_bus.h
)
add_test(NAME qindaqt.settings-input-shortcut-port
         COMMAND qindaqt_settings_input_shortcut_port_test)
set_tests_properties(
    qindaqt.settings-input-shortcut-port PROPERTIES
    LABELS "settings;input;dbus" TIMEOUT 60 RUN_SERIAL TRUE
)

# Route models over in-process fakes: presentation truth and write paths.
qindaqt_input_test(
    qindaqt_settings_input_pointer_devices_model_test
    SOURCES tst_pointer_devices_model.cpp
)
add_test(NAME qindaqt.settings-input-pointer-devices-model
         COMMAND qindaqt_settings_input_pointer_devices_model_test)
set_tests_properties(
    qindaqt.settings-input-pointer-devices-model PROPERTIES
    LABELS "settings;input" TIMEOUT 60
)

qindaqt_input_test(
    qindaqt_settings_input_keyboard_models_test
    SOURCES tst_input_keyboard_models.cpp
)
add_test(NAME qindaqt.settings-input-keyboard-models
         COMMAND qindaqt_settings_input_keyboard_models_test)
set_tests_properties(
    qindaqt.settings-input-keyboard-models PROPERTIES
    LABELS "settings;input" TIMEOUT 60
)

qindaqt_input_test(
    qindaqt_settings_input_shortcuts_model_test
    SOURCES tst_shortcuts_model.cpp
)
add_test(NAME qindaqt.settings-input-shortcuts-model
         COMMAND qindaqt_settings_input_shortcuts_model_test)
set_tests_properties(
    qindaqt.settings-input-shortcuts-model PROPERTIES
    LABELS "settings;input" TIMEOUT 60
)

# Offscreen Input page rows: capability hiding, conflict capture, the
# capture flow, and keyboard navigation over real models on fake ports.
qt_add_executable(
    qindaqt_settings_input_page_test
    tst_input_page.cpp
    support/fake_ports.h
)
target_include_directories(qindaqt_settings_input_page_test PRIVATE ${PROJECT_SOURCE_DIR})
target_link_libraries(
    qindaqt_settings_input_page_test PRIVATE
    qindaqt_settings_input
    qindaqt_design_tokens qindaqt_tokens_qml qindaqt_controls_qml
    Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Test
)
target_compile_definitions(
    qindaqt_settings_input_page_test PRIVATE
    QINDAQT_BUILD_QML_IMPORT_PATH="${CMAKE_BINARY_DIR}/qml"
    QINDAQT_INPUT_BUILD_QML_FILE="${CMAKE_BINARY_DIR}/qml/QindaQt/SettingsApp/Input/qml/InputPage.qml"
)
add_dependencies(
    qindaqt_settings_input_page_test
    qindaqt_settings_input_qml qindaqt_settings_input_qmlplugin
    qindaqt_controls_qml qindaqt_controls_qmlplugin qindaqt_tokens_qml qindaqt_tokens_qmlplugin
)
qindaqt_enable_warnings(qindaqt_settings_input_page_test)
add_test(NAME qindaqt.settings-input-page
         COMMAND qindaqt_settings_input_page_test)
set_tests_properties(
    qindaqt.settings-input-page PROPERTIES
    LABELS "offscreen;settings;input"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent;DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent"
)

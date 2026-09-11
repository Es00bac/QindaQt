# SPDX-License-Identifier: GPL-3.0-or-later

find_package(KF6Config 6.0 REQUIRED CONFIG)

# Domain ports and models. The KWin/kinconfig/kglobalaccel adapters are
# linked in through this target and only constructed by the composition
# root in the backend QML module.
qt_add_library(qindaqt_settings_input STATIC)
target_sources(
    qindaqt_settings_input
    PRIVATE
        evdev_layout_catalog.cpp
        kwin_pointer_device_port.cpp
        keyboard_layouts_model.cpp
        keyboard_settings_model.cpp
        pointer_device_selection.cpp
        pointer_devices_model.cpp
        config_change_announcement.cpp
        qt_keyboard_config_port.cpp
        qt_keyboard_layout_port.cpp
        qt_shortcut_port.cpp
        shortcuts_model.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/apps/settings_input/evdev_layout_catalog.h
            include/qindaqt/apps/settings_input/config_change_announcement.h
            include/qindaqt/apps/settings_input/keyboard_config_port.h
            include/qindaqt/apps/settings_input/keyboard_layout_port.h
            include/qindaqt/apps/settings_input/keyboard_layouts_model.h
            include/qindaqt/apps/settings_input/keyboard_settings_model.h
            include/qindaqt/apps/settings_input/pointer_device_port.h
            include/qindaqt/apps/settings_input/pointer_device_selection.h
            include/qindaqt/apps/settings_input/pointer_devices_model.h
            include/qindaqt/apps/settings_input/shortcut_port.h
            include/qindaqt/apps/settings_input/shortcuts_model.h
            include/qindaqt/apps/settings_input/store_result.h
)
target_link_libraries(
    qindaqt_settings_input
    PUBLIC KF6::ConfigCore Qt6::Core Qt6::DBus Qt6::Gui
)
target_compile_features(qindaqt_settings_input PUBLIC cxx_std_20)
set_target_properties(
    qindaqt_settings_input PROPERTIES
    CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON
)
qindaqt_enable_warnings(qindaqt_settings_input)

# Backend composition singleton. Main.qml imports this module so the
# Settings process constructor (main.cpp) stays out of input composition.
set(QINDAQT_INPUT_SETTINGS_BACKEND_QML_OUTPUT_DIRECTORY
    "${CMAKE_BINARY_DIR}/qml/QindaQt/SettingsApp/InputBackend")
qt_add_library(qindaqt_settings_input_backend_qml STATIC)
qt_add_qml_module(
    qindaqt_settings_input_backend_qml
    URI QindaQt.SettingsApp.InputBackend
    VERSION 1.0
    CLASS_NAME QindaQtSettingsInputBackendPlugin
    OUTPUT_DIRECTORY
        "${QINDAQT_INPUT_SETTINGS_BACKEND_QML_OUTPUT_DIRECTORY}"
    SOURCES
        input_route_composition.h
        input_route_composition.cpp
)
target_link_libraries(
    qindaqt_settings_input_backend_qml
    PRIVATE qindaqt_settings_input QindaQt::TokensQml
            Qt6::Core Qt6::DBus Qt6::Gui Qt6::Qml Qt6::Quick
)
set_target_properties(qindaqt_settings_input_backend_qml PROPERTIES
    CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON)
qindaqt_enable_warnings(qindaqt_settings_input_backend_qml)

# Route QML pages.
set(QINDAQT_INPUT_SETTINGS_QML_OUTPUT_DIRECTORY
    "${CMAKE_BINARY_DIR}/qml/QindaQt/SettingsApp/Input")
qt_add_library(qindaqt_settings_input_qml SHARED)
qt_add_qml_module(
    qindaqt_settings_input_qml
    URI QindaQt.SettingsApp.Input
    VERSION 1.0
    CLASS_NAME QindaQtSettingsInputPlugin
    OUTPUT_DIRECTORY "${QINDAQT_INPUT_SETTINGS_QML_OUTPUT_DIRECTORY}"
    TYPEINFO qindaqt_settings_input.qmltypes
    QML_FILES
        qml/InputKeyboardSection.qml
        qml/InputPage.qml
        qml/InputPointerSection.qml
        qml/InputShortcutRow.qml
        qml/InputShortcutsSection.qml
        qml/ShortcutCaptureButton.qml
)
target_link_libraries(
    qindaqt_settings_input_qml
    PRIVATE QindaQt::TokensQml
            Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick
            Qt6::QuickControls2
)
set_target_properties(
    qindaqt_settings_input_qml PROPERTIES
    CXX_EXTENSIONS OFF
    INSTALL_RPATH "$ORIGIN/../../Tokens;$ORIGIN/../../Controls"
)
qindaqt_enable_warnings(qindaqt_settings_input_qml)

# AGENT-NOTE: The route ships inside the shared SettingsAppearanceRuntime
# component like every other Settings route; a separate component would
# leave the Input module out of the relocated product.
install(
    TARGETS qindaqt_settings_input_qml qindaqt_settings_input_qmlplugin
    RUNTIME DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Input"
        COMPONENT SettingsAppearanceRuntime
    LIBRARY DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Input"
        COMPONENT SettingsAppearanceRuntime
    ARCHIVE DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Input"
        COMPONENT SettingsAppearanceRuntime
)
install(
    FILES
        "${QINDAQT_INPUT_SETTINGS_QML_OUTPUT_DIRECTORY}/qmldir"
        "${QINDAQT_INPUT_SETTINGS_QML_OUTPUT_DIRECTORY}/qindaqt_settings_input.qmltypes"
        qml/InputKeyboardSection.qml
        qml/InputPage.qml
        qml/InputPointerSection.qml
        qml/InputShortcutRow.qml
        qml/InputShortcutsSection.qml
        qml/ShortcutCaptureButton.qml
    DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Input"
    COMPONENT SettingsAppearanceRuntime
)

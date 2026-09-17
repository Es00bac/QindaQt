# SPDX-License-Identifier: LGPL-3.0-or-later

# Pure presentation and request values. The runtime target below is the only
# WizClient/QML composition boundary.
qt_add_library(qindaqt_shell_smart_lights_applet STATIC)
add_library(QindaQt::ShellSmartLightsApplet ALIAS qindaqt_shell_smart_lights_applet)

target_sources(
    qindaqt_shell_smart_lights_applet
    PRIVATE
        src/smart_lights_applet_presentation.cpp
        src/smart_lights_request_state.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/shell/smart_lights_applet/smart_lights_applet_presentation.h
            include/qindaqt/shell/smart_lights_applet/smart_lights_applet_types.h
            include/qindaqt/shell/smart_lights_applet/smart_lights_request_state.h
)
target_link_libraries(
    qindaqt_shell_smart_lights_applet
    PUBLIC QindaQt::SmartLightsStore QindaQt::WizProtocol Qt6::Core
)
target_compile_features(qindaqt_shell_smart_lights_applet PUBLIC cxx_std_20)
set_target_properties(
    qindaqt_shell_smart_lights_applet PROPERTIES CXX_EXTENSIONS OFF
)
qindaqt_enable_warnings(qindaqt_shell_smart_lights_applet)

find_package(Qt6 6.11 REQUIRED COMPONENTS Qml Quick QuickControls2 Svg)

qt_add_library(qindaqt_shell_smart_lights_applet_runtime STATIC)
add_library(QindaQt::ShellSmartLightsAppletRuntime ALIAS
            qindaqt_shell_smart_lights_applet_runtime)
target_sources(
    qindaqt_shell_smart_lights_applet_runtime
    PRIVATE
        src/smart_lights_applet_control_ops.cpp
        src/smart_lights_applet_controller.cpp
        src/smart_lights_applet_controller.h
        src/smart_lights_applet_presets.cpp
)
qt_add_qml_module(
    qindaqt_shell_smart_lights_applet_runtime
    URI QindaQt.Shell.SmartLightsApplet
    VERSION 1.0
    OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/qml/QindaQt/Shell/SmartLightsApplet"
    QML_FILES
        qml/SmartLightDeviceControls.qml
        qml/SmartLightDeviceRow.qml
        qml/SmartLightPresetRow.qml
        qml/SmartLightsApplet.qml
    DEPENDENCIES
        TARGET qindaqt_shell_icons
)
target_include_directories(
    qindaqt_shell_smart_lights_applet_runtime
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
target_link_libraries(
    qindaqt_shell_smart_lights_applet_runtime
    PUBLIC
        QindaQt::ShellIcons
        QindaQt::ShellSmartLightsApplet
        QindaQt::SmartLightsStore
        QindaQt::WizClient
        Qt6::Core
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
)
target_compile_features(qindaqt_shell_smart_lights_applet_runtime PUBLIC cxx_std_20)
set_target_properties(qindaqt_shell_smart_lights_applet_runtime
                      PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_shell_smart_lights_applet_runtime)

# The compiled module imports QindaQt.Controls; that module must be built
# before qmlcachegen runs. src/controls is processed before this directory
# (Bluetooth applet precedent), so the target already exists here.
add_dependencies(qindaqt_shell_smart_lights_applet_runtime qindaqt_controls_qmlplugin)

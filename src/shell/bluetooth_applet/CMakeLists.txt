# SPDX-License-Identifier: LGPL-3.0-or-later

# Pure presentation and request values. The runtime target below is the only
# BluetoothClient/QML composition boundary.
qt_add_library(qindaqt_shell_bluetooth_applet STATIC)
add_library(QindaQt::ShellBluetoothApplet ALIAS qindaqt_shell_bluetooth_applet)

target_sources(
    qindaqt_shell_bluetooth_applet
    PRIVATE
        src/bluetooth_applet_presentation.cpp
        src/bluetooth_request_state.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/shell/bluetooth_applet/bluetooth_applet_presentation.h
            include/qindaqt/shell/bluetooth_applet/bluetooth_applet_types.h
            include/qindaqt/shell/bluetooth_applet/bluetooth_request_state.h
)
target_link_libraries(
    qindaqt_shell_bluetooth_applet
    PUBLIC QindaQt::BluetoothProtocol Qt6::Core
)
target_compile_features(qindaqt_shell_bluetooth_applet PUBLIC cxx_std_20)
set_target_properties(qindaqt_shell_bluetooth_applet PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_shell_bluetooth_applet)

find_package(Qt6 6.11 REQUIRED COMPONENTS Qml Quick QuickControls2 Svg)

qt_add_library(qindaqt_shell_bluetooth_applet_runtime STATIC)
add_library(QindaQt::ShellBluetoothAppletRuntime ALIAS
            qindaqt_shell_bluetooth_applet_runtime)
target_sources(
    qindaqt_shell_bluetooth_applet_runtime
    PRIVATE
        src/bluetooth_applet_controller.cpp
        src/bluetooth_applet_controller.h
        src/bluetooth_applet_pairing.cpp
)
qt_add_qml_module(
    qindaqt_shell_bluetooth_applet_runtime
    URI QindaQt.Shell.BluetoothApplet
    VERSION 1.0
    OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/qml/QindaQt/Shell/BluetoothApplet"
    QML_FILES
        qml/BluetoothAdapterRow.qml
        qml/BluetoothApplet.qml
        qml/BluetoothDeviceRow.qml
        qml/BluetoothPairingPrompt.qml
    DEPENDENCIES
        TARGET qindaqt_shell_icons
)
target_include_directories(
    qindaqt_shell_bluetooth_applet_runtime PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
target_link_libraries(
    qindaqt_shell_bluetooth_applet_runtime
    PUBLIC
        QindaQt::BluetoothClient
        QindaQt::ShellBluetoothApplet
        QindaQt::ShellIcons
        Qt6::Core
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
)
target_compile_features(qindaqt_shell_bluetooth_applet_runtime PUBLIC cxx_std_20)
set_target_properties(qindaqt_shell_bluetooth_applet_runtime
                      PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_shell_bluetooth_applet_runtime)

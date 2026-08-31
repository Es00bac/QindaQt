# SPDX-License-Identifier: LGPL-3.0-or-later

# Pure presentation model. Keep this target dependency-light; the separate
# runtime target below is the only PowerClient/QML composition boundary.

qt_add_library(qindaqt_shell_power_applet STATIC)
add_library(QindaQt::ShellPowerApplet ALIAS qindaqt_shell_power_applet)

target_sources(
    qindaqt_shell_power_applet
    PRIVATE
        src/brightness_request_state.cpp
        src/power_applet_presentation.cpp
        src/power_control_rows.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/shell/power_applet/brightness_request_state.h
            include/qindaqt/shell/power_applet/power_applet_presentation.h
            include/qindaqt/shell/power_applet/power_applet_types.h
)
target_link_libraries(
    qindaqt_shell_power_applet
    PUBLIC QindaQt::PowerProtocol QindaQt::BrightnessModel Qt6::Core
)
target_compile_features(qindaqt_shell_power_applet PUBLIC cxx_std_20)
set_target_properties(qindaqt_shell_power_applet PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_shell_power_applet)

find_package(Qt6 6.11 REQUIRED COMPONENTS Qml Quick QuickControls2)

qt_add_library(qindaqt_shell_power_applet_runtime STATIC)
add_library(QindaQt::ShellPowerAppletRuntime ALIAS
            qindaqt_shell_power_applet_runtime)
target_sources(
    qindaqt_shell_power_applet_runtime
    PRIVATE
        src/power_applet_controller.cpp
        src/power_applet_controller.h
)
qt_add_qml_module(
    qindaqt_shell_power_applet_runtime
    URI QindaQt.Shell.PowerApplet
    VERSION 1.0
    OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/qml/QindaQt/Shell/PowerApplet"
    QML_FILES qml/PowerApplet.qml
)
target_include_directories(
    qindaqt_shell_power_applet_runtime PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
target_link_libraries(
    qindaqt_shell_power_applet_runtime
    PUBLIC
        QindaQt::ShellPowerApplet
        QindaQt::PowerClient
        Qt6::Core
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
)
target_compile_features(qindaqt_shell_power_applet_runtime PUBLIC cxx_std_20)
set_target_properties(qindaqt_shell_power_applet_runtime
                      PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_shell_power_applet_runtime)

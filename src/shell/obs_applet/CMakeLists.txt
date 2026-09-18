# SPDX-License-Identifier: GPL-3.0-or-later

# Pure presentation: one snapshot becomes one top bar. No client, no QML.
qt_add_library(qindaqt_shell_obs_applet STATIC)
add_library(QindaQt::ShellObsApplet ALIAS qindaqt_shell_obs_applet)

target_sources(
    qindaqt_shell_obs_applet
    PRIVATE
        src/obs_applet_presentation.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/shell/obs_applet/obs_applet_presentation.h
)
target_link_libraries(
    qindaqt_shell_obs_applet PUBLIC QindaQt::ObsClient Qt6::Core
)
target_compile_features(qindaqt_shell_obs_applet PUBLIC cxx_std_20)
set_target_properties(qindaqt_shell_obs_applet PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_shell_obs_applet)

find_package(Qt6 6.11 REQUIRED COMPONENTS Qml Quick QuickControls2)

# The runtime target is the only ObsClient/QML composition boundary.
qt_add_library(qindaqt_shell_obs_applet_runtime STATIC)
add_library(QindaQt::ShellObsAppletRuntime ALIAS qindaqt_shell_obs_applet_runtime)
target_sources(
    qindaqt_shell_obs_applet_runtime
    PRIVATE
        src/obs_applet_controller.cpp
        src/obs_applet_controller.h
)
qt_add_qml_module(
    qindaqt_shell_obs_applet_runtime
    URI QindaQt.Shell.ObsApplet
    VERSION 1.0
    OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/qml/QindaQt/Shell/ObsApplet"
    DEPENDENCIES
        TARGET qindaqt_shell_icons
    QML_FILES
        qml/ObsApplet.qml
        qml/ObsAppletPopup.qml
        qml/ObsAppletToggleRow.qml
)
target_include_directories(
    qindaqt_shell_obs_applet_runtime PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
target_link_libraries(
    qindaqt_shell_obs_applet_runtime
    PUBLIC
        QindaQt::ShellIcons
        QindaQt::ShellObsApplet
        QindaQt::ObsClient
        Qt6::Core
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
)
target_compile_features(qindaqt_shell_obs_applet_runtime PUBLIC cxx_std_20)
set_target_properties(qindaqt_shell_obs_applet_runtime
                      PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_shell_obs_applet_runtime)

# The compiled module imports QindaQt.Controls; that module must be built
# before qmlcachegen runs (same precedent as the Bluetooth and smart-lights
# applets).
add_dependencies(qindaqt_shell_obs_applet_runtime qindaqt_controls_qmlplugin)

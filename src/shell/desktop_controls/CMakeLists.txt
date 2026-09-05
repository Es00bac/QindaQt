# SPDX-License-Identifier: GPL-3.0-or-later

# Desktop controls: the stock-preset controls that are compositions over
# EXISTING shell facades (launcher, global menu, task list, audio, Bluetooth,
# power, session actions) plus the workspace module. The controller target
# holds the QML-facing facades and pure projections; the runtime target adds
# the compiled QindaQt.Shell.DesktopControls 1.0 module. Both stay STATIC like
# every other in-process audited applet (see docs/wiki/shell/desktop-controls.md).

find_package(Qt6 6.11 REQUIRED COMPONENTS Core DBus Gui Qml Quick QuickControls2 Svg)

# The launcher runtime is normally materialized at the end of the src scope;
# the call is idempotent and lets this directory link it deterministically.
qindaqt_add_launcher_runtime()

qt_add_library(qindaqt_shell_desktop_controls STATIC)
add_library(QindaQt::ShellDesktopControls ALIAS qindaqt_shell_desktop_controls)
target_sources(
    qindaqt_shell_desktop_controls
    PRIVATE
        src/active_application_controller.cpp
        src/applet_grants.cpp
        src/command_search_controller.cpp
        src/command_search_types.cpp
        src/desktop_controls_access.cpp
        src/file_manager_folder_opener.cpp
        src/places_controller.cpp
        src/quick_launch_controller.cpp
        src/system_menu_controller.cpp
        src/system_status_controller.cpp
        src/system_status_presentation.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/shell/desktop_controls/active_application_controller.h
            include/qindaqt/shell/desktop_controls/applet_grants.h
            include/qindaqt/shell/desktop_controls/command_search_controller.h
            include/qindaqt/shell/desktop_controls/command_search_types.h
            include/qindaqt/shell/desktop_controls/desktop_controls_access.h
            include/qindaqt/shell/desktop_controls/file_manager_folder_opener.h
            include/qindaqt/shell/desktop_controls/places_controller.h
            include/qindaqt/shell/desktop_controls/quick_launch_controller.h
            include/qindaqt/shell/desktop_controls/system_menu_controller.h
            include/qindaqt/shell/desktop_controls/system_status_controller.h
            include/qindaqt/shell/desktop_controls/system_status_presentation.h
)
# AGENT-CONTRACT: this target consumes only the public facades of the applets
# it aggregates plus the workspace controller. It must never link a service
# client, a D-Bus transport, KWin, LayerShellQt, or the shell runtime; the
# boundary test in tests/shell/desktop_controls enforces the include set.
target_link_libraries(
    qindaqt_shell_desktop_controls
    PUBLIC
        QindaQt::ShellWorkspaces
        qindaqt_applets
        QindaQt::AppletHost
        QindaQt::AppletRuntime
        QindaQt::ShellLauncherRuntime
        QindaQt::GlobalMenuApplet
        QindaQt::ShellTaskListApplet
        QindaQt::ShellAudioAppletRuntime
        QindaQt::ShellBluetoothAppletRuntime
        QindaQt::ShellPowerAppletRuntime
        Qt6::Core
)
target_compile_features(qindaqt_shell_desktop_controls PUBLIC cxx_std_20)
set_target_properties(qindaqt_shell_desktop_controls PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_shell_desktop_controls)

set(QINDAQT_DESKTOP_CONTROLS_QML_OUTPUT_DIRECTORY
    "${CMAKE_BINARY_DIR}/qml/QindaQt/Shell/DesktopControls")

qt_add_library(qindaqt_shell_desktop_controls_runtime STATIC)
add_library(QindaQt::ShellDesktopControlsRuntime ALIAS
            qindaqt_shell_desktop_controls_runtime)
# Presentation files are listed by name (not GLOB) so the install helper in
# src/shell/DesktopControlsRuntimeInstall.cmake and the closure probe can pin
# the same inventory.
set(QINDAQT_DESKTOP_CONTROLS_QML_FILES
    qml/ActiveApplicationApplet.qml
    qml/ApplicationTilesApplet.qml
    qml/CommandHudApplet.qml
    qml/CommandPaletteApplet.qml
    qml/CommandSearchPopup.qml
    qml/ControlPopupFrame.qml
    qml/DashboardApplet.qml
    qml/DesktopControlButton.qml
    qml/MenuRow.qml
    qml/OverviewTriggerApplet.qml
    qml/PlacesMenuApplet.qml
    qml/QuickLaunchApplet.qml
    qml/ShowDesktopApplet.qml
    qml/StatusLaneRow.qml
    qml/SystemMenuApplet.qml
    qml/SystemStatusApplet.qml
    qml/WorkspaceStrip.qml
    qml/WorkspaceSwitcherApplet.qml
    qml/WorkspaceTilesApplet.qml
)
set_property(GLOBAL PROPERTY QINDAQT_DESKTOP_CONTROLS_QML_FILES
             "${QINDAQT_DESKTOP_CONTROLS_QML_FILES}")
qt_add_qml_module(
    qindaqt_shell_desktop_controls_runtime
    URI QindaQt.Shell.DesktopControls
    VERSION 1.0
    OUTPUT_DIRECTORY "${QINDAQT_DESKTOP_CONTROLS_QML_OUTPUT_DIRECTORY}"
    TYPEINFO qindaqt_shell_desktop_controls.qmltypes
    QML_FILES ${QINDAQT_DESKTOP_CONTROLS_QML_FILES}
    DEPENDENCIES
        TARGET qindaqt_shell_icons
)
target_link_libraries(
    qindaqt_shell_desktop_controls_runtime
    PUBLIC
        QindaQt::ShellDesktopControls
        QindaQt::ShellIcons
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
)
target_compile_features(qindaqt_shell_desktop_controls_runtime PUBLIC cxx_std_20)
set_target_properties(qindaqt_shell_desktop_controls_runtime
                      PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_shell_desktop_controls_runtime)
add_dependencies(
    qindaqt_shell_desktop_controls_runtime
    qindaqt_controls_qmlplugin
    qindaqt_tokens_qmlplugin
    qindaqt_shell_iconsplugin
)

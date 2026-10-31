# SPDX-License-Identifier: GPL-3.0-or-later

# Pure presentation over the public Firewall1 value model (ADR-0431,
# docs/wiki/shell/firewall-applet.md). The runtime target below is the only
# FirewallClient/QML composition boundary; shell composition in
# src/shell/runtime owns the Qt D-Bus transport and client.
qt_add_library(qindaqt_shell_firewall_applet STATIC)
add_library(QindaQt::ShellFirewallApplet ALIAS qindaqt_shell_firewall_applet)

target_sources(
    qindaqt_shell_firewall_applet
    PRIVATE
        src/firewall_applet_presentation.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/shell/firewall_applet/firewall_applet_presentation.h
            include/qindaqt/shell/firewall_applet/firewall_applet_types.h
)
target_link_libraries(
    qindaqt_shell_firewall_applet
    PUBLIC QindaQt::FirewallProtocol Qt6::Core
)
target_compile_features(qindaqt_shell_firewall_applet PUBLIC cxx_std_20)
set_target_properties(qindaqt_shell_firewall_applet PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_shell_firewall_applet)

find_package(Qt6 6.11 REQUIRED COMPONENTS Qml Quick QuickControls2 Svg)

qt_add_library(qindaqt_shell_firewall_applet_runtime STATIC)
add_library(QindaQt::ShellFirewallAppletRuntime ALIAS
            qindaqt_shell_firewall_applet_runtime)
target_sources(
    qindaqt_shell_firewall_applet_runtime
    PRIVATE
        src/firewall_applet_controller.cpp
        src/firewall_applet_controller.h
)
qt_add_qml_module(
    qindaqt_shell_firewall_applet_runtime
    URI QindaQt.Shell.FirewallApplet
    VERSION 1.0
    OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/qml/QindaQt/Shell/FirewallApplet"
    QML_FILES
        qml/FirewallApplet.qml
    DEPENDENCIES
        TARGET qindaqt_shell_icons
)
target_include_directories(
    qindaqt_shell_firewall_applet_runtime PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
# AGENT-CONTRACT: the runtime consumes only the public FirewallClient. It
# must never link the Qt D-Bus transport or the QindaFirewall service
# itself; shell composition owns the transport (firewallappletcomposition).
target_link_libraries(
    qindaqt_shell_firewall_applet_runtime
    PUBLIC
        QindaQt::FirewallClient
        QindaQt::ShellFirewallApplet
        QindaQt::ShellIcons
        Qt6::Core
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
)
target_compile_features(qindaqt_shell_firewall_applet_runtime PUBLIC cxx_std_20)
set_target_properties(qindaqt_shell_firewall_applet_runtime
                      PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_shell_firewall_applet_runtime)

# The compiled module imports QindaQt.Controls; that module must be built
# before qmlcachegen runs (Bluetooth applet precedent).
add_dependencies(qindaqt_shell_firewall_applet_runtime qindaqt_controls_qmlplugin)

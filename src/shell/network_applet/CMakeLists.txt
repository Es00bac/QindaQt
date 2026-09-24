# SPDX-License-Identifier: LGPL-3.0-or-later

# Pure presentation and request values over the public Network1 value model.
# The runtime target below is the only NetworkClient/QML composition boundary
# (docs/wiki/shell/network-applet.md, ADR-0258).
qt_add_library(qindaqt_shell_network_applet STATIC)
add_library(QindaQt::ShellNetworkApplet ALIAS qindaqt_shell_network_applet)

target_sources(
    qindaqt_shell_network_applet
    PRIVATE
        src/network_applet_presentation.cpp
        src/network_request_state.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/shell/network_applet/network_applet_presentation.h
            include/qindaqt/shell/network_applet/network_applet_types.h
            include/qindaqt/shell/network_applet/network_request_state.h
)
target_link_libraries(
    qindaqt_shell_network_applet
    PUBLIC QindaQt::NetworkModel QindaQt::NetworkProtocol Qt6::Core
)
target_compile_features(qindaqt_shell_network_applet PUBLIC cxx_std_20)
set_target_properties(qindaqt_shell_network_applet PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_shell_network_applet)

find_package(Qt6 6.11 REQUIRED COMPONENTS Qml Quick QuickControls2 Svg)

qt_add_library(qindaqt_shell_network_applet_runtime STATIC)
add_library(QindaQt::ShellNetworkAppletRuntime ALIAS
            qindaqt_shell_network_applet_runtime)
target_sources(
    qindaqt_shell_network_applet_runtime
    PRIVATE
        src/network_applet_actions.cpp
        src/network_applet_controller.cpp
        src/network_applet_controller.h
)
qt_add_qml_module(
    qindaqt_shell_network_applet_runtime
    URI QindaQt.Shell.NetworkApplet
    VERSION 1.0
    OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/qml/QindaQt/Shell/NetworkApplet"
    QML_FILES
        qml/NetworkAccessPointRow.qml
        qml/NetworkApplet.qml
        qml/NetworkConnectionRow.qml
    DEPENDENCIES
        TARGET qindaqt_shell_icons
)
target_include_directories(
    qindaqt_shell_network_applet_runtime PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
# AGENT-CONTRACT: the runtime consumes only the public NetworkClient. It must
# never link the Qt D-Bus transport, the service, or the NetworkManager
# adapter; shell composition owns the transport (networkappletcomposition).
target_link_libraries(
    qindaqt_shell_network_applet_runtime
    PUBLIC
        QindaQt::NetworkClient
        QindaQt::ShellNetworkApplet
        QindaQt::ShellIcons
        Qt6::Core
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
)
target_compile_features(qindaqt_shell_network_applet_runtime PUBLIC cxx_std_20)
set_target_properties(qindaqt_shell_network_applet_runtime
                      PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_shell_network_applet_runtime)

# The compiled module imports QindaQt.Controls; that module must be built
# before qmlcachegen runs (Bluetooth applet precedent).
add_dependencies(qindaqt_shell_network_applet_runtime qindaqt_controls_qmlplugin)

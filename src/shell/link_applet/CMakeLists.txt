# SPDX-License-Identifier: GPL-3.0-or-later

# Pure presentation over the public Link1 value model (ADR-0435,
# docs/wiki/shell/link-applet.md). The runtime target below is the only
# LinkClient/QML composition boundary; shell composition in
# src/shell/runtime owns the Qt D-Bus transport and client.
qt_add_library(qindaqt_shell_link_applet STATIC)
add_library(QindaQt::ShellLinkApplet ALIAS qindaqt_shell_link_applet)

target_sources(
    qindaqt_shell_link_applet
    PRIVATE
        src/link_applet_presentation.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/shell/link_applet/link_applet_presentation.h
            include/qindaqt/shell/link_applet/link_applet_types.h
)
target_link_libraries(
    qindaqt_shell_link_applet
    PUBLIC QindaQt::LinkClient Qt6::Core
)
target_compile_features(qindaqt_shell_link_applet PUBLIC cxx_std_20)
set_target_properties(qindaqt_shell_link_applet PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_shell_link_applet)

find_package(Qt6 6.11 REQUIRED COMPONENTS Qml Quick QuickControls2 Svg)

qt_add_library(qindaqt_shell_link_applet_runtime STATIC)
add_library(QindaQt::ShellLinkAppletRuntime ALIAS
            qindaqt_shell_link_applet_runtime)
target_sources(
    qindaqt_shell_link_applet_runtime
    PRIVATE
        src/link_applet_controller.cpp
        src/link_applet_controller.h
)
qt_add_qml_module(
    qindaqt_shell_link_applet_runtime
    URI QindaQt.Shell.LinkApplet
    VERSION 1.0
    OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/qml/QindaQt/Shell/LinkApplet"
    QML_FILES
        qml/LinkApplet.qml
        qml/LinkPairingSection.qml
    DEPENDENCIES
        TARGET qindaqt_shell_icons
)
target_include_directories(
    qindaqt_shell_link_applet_runtime PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
# AGENT-CONTRACT: the runtime consumes only the public LinkClient. It
# must never include QtLinkTransport or reach the link service; shell
# composition owns the transport (linkappletcomposition).
target_link_libraries(
    qindaqt_shell_link_applet_runtime
    PUBLIC
        QindaQt::LinkClient
        QindaQt::ShellLinkApplet
        QindaQt::ShellIcons
        Qt6::Core
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
)
target_compile_features(qindaqt_shell_link_applet_runtime PUBLIC cxx_std_20)
set_target_properties(qindaqt_shell_link_applet_runtime
                      PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_shell_link_applet_runtime)

# The compiled module imports QindaQt.Controls; that module must be built
# before qmlcachegen runs (Bluetooth applet precedent).
add_dependencies(qindaqt_shell_link_applet_runtime qindaqt_controls_qmlplugin)

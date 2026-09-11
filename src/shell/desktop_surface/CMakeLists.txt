# SPDX-License-Identifier: GPL-3.0-or-later

# Desktop surface (ADR-0125): the per-output background-layer surface hosting
# the desktop-icons applet. The controller target owns window/layer policy and
# desktop-inventory resolution; the runtime target adds the compiled
# QindaQt.Shell.DesktopSurface 1.0 module with the QML-facing
# NewFolderController. Both stay STATIC like every other in-process audited
# applet (desktop-controls precedent). Presentation only borrows facades:
# neither target may link a service client, a D-Bus transport, KWin, or the
# shell runtime; LayerShellQt is confined to the controller's window setup.

find_package(Qt6 6.11 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2)
find_package(LayerShellQt 6.6.6 REQUIRED)

qt_add_library(qindaqt_desktop_surface STATIC)
add_library(QindaQt::ShellDesktopSurface ALIAS qindaqt_desktop_surface)
target_sources(
    qindaqt_desktop_surface
    PRIVATE
        src/desktop_surface_controller.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/shell/desktop_surface/desktop_surface_controller.h
)
target_link_libraries(
    qindaqt_desktop_surface
    PUBLIC
        QindaQt::AppletHost
        QindaQt::AppletRuntime
        qindaqt_applets
        qindaqt_profiles
        Qt6::Core
        Qt6::Gui
        Qt6::Quick
        LayerShellQt::Interface
)
target_compile_features(qindaqt_desktop_surface PUBLIC cxx_std_20)
set_target_properties(qindaqt_desktop_surface PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_desktop_surface)

set(QINDAQT_DESKTOP_SURFACE_QML_OUTPUT_DIRECTORY
    "${CMAKE_BINARY_DIR}/qml/QindaQt/Shell/DesktopSurface")

qt_add_library(qindaqt_desktop_surface_qml STATIC)
add_library(QindaQt::ShellDesktopSurfaceQml ALIAS qindaqt_desktop_surface_qml)
qt_add_qml_module(
    qindaqt_desktop_surface_qml
    URI QindaQt.Shell.DesktopSurface
    VERSION 1.0
    OUTPUT_DIRECTORY "${QINDAQT_DESKTOP_SURFACE_QML_OUTPUT_DIRECTORY}"
    TYPEINFO qindaqt_desktop_surface.qmltypes
    SOURCES
        include/qindaqt/shell/desktop_surface/new_folder_controller.h
        src/new_folder_controller.cpp
    QML_FILES
        qml/DesktopApplicationsMenu.qml
        qml/DesktopContextMenu.qml
        qml/DesktopIconsView.qml
        qml/DesktopSurface.qml
    DEPENDENCIES
        TARGET qindaqt_shell_icons
)
# The generated QML type-registration unit includes the QML_ELEMENT header by
# its file name (icons precedent); expose its directory privately.
target_include_directories(
    qindaqt_desktop_surface_qml
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include/qindaqt/shell/desktop_surface"
)
target_link_libraries(
    qindaqt_desktop_surface_qml
    PUBLIC
        QindaQt::ShellDesktopSurface
        QindaQt::ShellIcons
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
)
target_compile_features(qindaqt_desktop_surface_qml PUBLIC cxx_std_20)
set_target_properties(qindaqt_desktop_surface_qml
                      PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_desktop_surface_qml)
add_dependencies(qindaqt_desktop_surface_qml qindaqt_shell_iconsplugin)

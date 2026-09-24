# SPDX-License-Identifier: GPL-3.0-or-later

# ADR-0260: the shell-owned desktop menu provider - the File Manager's menu
# shown in the global menu while no application is active. Pure builder,
# selection/activation controller, and the desktop-icons command channel.
# It depends only on the global-menu applet facade and canonical protocol plus
# File Manager's public menu catalog; shell runtime composes it over the
# existing controllers through the DesktopMenuTargets port.

find_package(Qt6 6.11 REQUIRED COMPONENTS Core Gui)

qt_add_library(qindaqt_shell_desktop_menu STATIC)
add_library(QindaQt::ShellDesktopMenu ALIAS qindaqt_shell_desktop_menu)
target_sources(
    qindaqt_shell_desktop_menu
    PRIVATE
        src/desktop_menu_controller.cpp
        src/desktop_menu_model.cpp
        src/desktop_surface_commands.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/shell/desktop_menu/desktop_menu_controller.h
            include/qindaqt/shell/desktop_menu/desktop_menu_model.h
            include/qindaqt/shell/desktop_menu/desktop_menu_targets.h
            include/qindaqt/shell/desktop_menu/desktop_menu_types.h
            include/qindaqt/shell/desktop_menu/desktop_surface_commands.h
)
# AGENT-CONTRACT: never link a service client, a D-Bus transport, KWin,
# LayerShellQt, QML, a process launcher, or the shell runtime here. Every
# effect leaves through DesktopMenuTargets, which shell runtime implements
# over the controllers that already own each action.
target_link_libraries(
    qindaqt_shell_desktop_menu
    PUBLIC
        QindaQt::GlobalMenuApplet
        QindaQt::GlobalMenuProtocol
        Qt6::Core
    PRIVATE
        qindaqt_file_manager_menu_catalog
)
target_compile_features(qindaqt_shell_desktop_menu PUBLIC cxx_std_20)
set_target_properties(qindaqt_shell_desktop_menu PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_shell_desktop_menu)

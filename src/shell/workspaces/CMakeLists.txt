# SPDX-License-Identifier: LGPL-3.0-or-later

# Workspace (virtual desktop) truth for the desktop controls. The pure target
# owns bounded values, validation, the transport seam, and the QML-facing
# controller; the separate D-Bus target is the only place that names KWin's
# public bus surface (ADR-0075). Both stay STATIC like the other applet
# collaborators composed in-process by the audited shell.

qt_add_library(qindaqt_shell_workspaces STATIC)
add_library(QindaQt::ShellWorkspaces ALIAS qindaqt_shell_workspaces)
target_sources(
    qindaqt_shell_workspaces
    PRIVATE
        src/workspace_types.cpp
        src/workspace_controller.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/shell/workspaces/workspace_types.h
            include/qindaqt/shell/workspaces/workspace_transport.h
            include/qindaqt/shell/workspaces/workspace_controller.h
)
# AGENT-CONTRACT: Qt Core only. The controller must never learn bus names,
# KWin objects, or QML; the boundary test in tests/shell/workspaces enforces it.
target_link_libraries(qindaqt_shell_workspaces PUBLIC Qt6::Core)
target_compile_features(qindaqt_shell_workspaces PUBLIC cxx_std_20)
set_target_properties(qindaqt_shell_workspaces PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_shell_workspaces)

find_package(Qt6 6.11 REQUIRED COMPONENTS DBus)

qt_add_library(qindaqt_shell_workspaces_dbus STATIC)
add_library(QindaQt::ShellWorkspacesDBus ALIAS qindaqt_shell_workspaces_dbus)
target_sources(
    qindaqt_shell_workspaces_dbus
    PRIVATE
        src/qt_workspace_transport.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/shell/workspaces/qt_workspace_transport.h
)
target_link_libraries(
    qindaqt_shell_workspaces_dbus
    PUBLIC QindaQt::ShellWorkspaces Qt6::Core Qt6::DBus
)
target_compile_features(qindaqt_shell_workspaces_dbus PUBLIC cxx_std_20)
set_target_properties(qindaqt_shell_workspaces_dbus PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_shell_workspaces_dbus)

# SPDX-License-Identifier: GPL-3.0-or-later

# Workspace module tests: pure values, the controller over a scripted
# transport, the authenticated Qt D-Bus adapter over a private dbus-daemon
# hosting a fake KWin, and the include-boundary probe. Nothing here touches
# the host session bus or a real compositor.

find_package(Qt6 6.11 REQUIRED COMPONENTS Core DBus Test)
find_program(DBUS_DAEMON_EXECUTABLE dbus-daemon REQUIRED)

set(workspace_test_environment
    "DBUS_SESSION_BUS_ADDRESS=unset:;DISPLAY=unset:;WAYLAND_DISPLAY=unset:;XDG_RUNTIME_DIR=set:${CMAKE_CURRENT_BINARY_DIR}/isolated-runtime")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/isolated-runtime")
file(CHMOD "${CMAKE_CURRENT_BINARY_DIR}/isolated-runtime"
     PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)

qt_add_executable(qindaqt_workspace_types_tests tst_workspace_types.cpp)
target_link_libraries(qindaqt_workspace_types_tests
    PRIVATE QindaQt::ShellWorkspaces Qt6::Test)
qindaqt_enable_warnings(qindaqt_workspace_types_tests)
add_test(NAME qindaqt.workspaces-values COMMAND qindaqt_workspace_types_tests)
set_tests_properties(qindaqt.workspaces-values PROPERTIES LABELS "shell;workspaces;pure")

qt_add_executable(qindaqt_workspace_controller_tests
    tst_workspace_controller.cpp
    fake_workspace_transport.h)
target_link_libraries(qindaqt_workspace_controller_tests
    PRIVATE QindaQt::ShellWorkspaces Qt6::Test)
qindaqt_enable_warnings(qindaqt_workspace_controller_tests)
add_test(NAME qindaqt.workspaces-controller COMMAND qindaqt_workspace_controller_tests)
set_tests_properties(qindaqt.workspaces-controller PROPERTIES LABELS "shell;workspaces")

qt_add_executable(qindaqt_workspace_qt_transport_tests
    tst_qt_workspace_transport.cpp)
target_link_libraries(qindaqt_workspace_qt_transport_tests
    PRIVATE QindaQt::ShellWorkspacesDBus Qt6::DBus Qt6::Test)
target_compile_definitions(qindaqt_workspace_qt_transport_tests
    PRIVATE QINDAQT_DBUS_DAEMON_EXECUTABLE="${DBUS_DAEMON_EXECUTABLE}")
qindaqt_enable_warnings(qindaqt_workspace_qt_transport_tests)
add_test(NAME qindaqt.workspaces-qt-transport-private-bus
         COMMAND qindaqt_workspace_qt_transport_tests)
set_tests_properties(qindaqt.workspaces-qt-transport-private-bus
    PROPERTIES
        LABELS "shell;workspaces;dbus;private-bus"
        TIMEOUT 60
        ENVIRONMENT_MODIFICATION "${workspace_test_environment}")

add_test(
    NAME qindaqt.workspaces-boundary
    COMMAND "${CMAKE_COMMAND}"
        "-DSOURCE_ROOT=${PROJECT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/check_workspaces_boundary.cmake"
)
set_tests_properties(qindaqt.workspaces-boundary PROPERTIES LABELS "shell;workspaces;policy")

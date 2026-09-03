# SPDX-License-Identifier: LGPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS DBus)

qt_add_library(qindaqt_shell_window_actions_client STATIC)
add_library(QindaQt::ShellWindowActionsClient ALIAS qindaqt_shell_window_actions_client)
target_sources(
    qindaqt_shell_window_actions_client
    PRIVATE
        src/shell_window_actions_client.cpp
        src/qt_shell_window_actions_transport.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/shell_window_actions_client/shell_window_actions_client.h
            include/qindaqt/shell_window_actions_client/shell_window_actions_transport.h
            include/qindaqt/shell_window_actions_client/qt_shell_window_actions_transport.h
)
target_compile_features(qindaqt_shell_window_actions_client PUBLIC cxx_std_20)
target_link_libraries(
    qindaqt_shell_window_actions_client
    PUBLIC QindaQt::CompositorShellActions Qt6::Core Qt6::DBus
)
set_target_properties(qindaqt_shell_window_actions_client PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_shell_window_actions_client)

install(
    TARGETS qindaqt_shell_window_actions_client
    EXPORT QindaQtTargets
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

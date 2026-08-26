# SPDX-License-Identifier: LGPL-3.0-or-later

add_library(qindaqt_shell_visibility_protocol INTERFACE)
add_library(QindaQt::ShellVisibilityProtocol ALIAS qindaqt_shell_visibility_protocol)

target_sources(
    qindaqt_shell_visibility_protocol
    INTERFACE
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/shell_visibility_protocol/wire_limits.h
)
target_link_libraries(qindaqt_shell_visibility_protocol INTERFACE Qt6::Core)
target_compile_features(qindaqt_shell_visibility_protocol INTERFACE cxx_std_20)

install(
    TARGETS qindaqt_shell_visibility_protocol
    EXPORT QindaQtTargets
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

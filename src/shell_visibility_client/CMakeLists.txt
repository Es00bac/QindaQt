# SPDX-License-Identifier: LGPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS DBus)

qt_add_library(qindaqt_shell_visibility_client STATIC)
add_library(QindaQt::ShellVisibilityClient ALIAS qindaqt_shell_visibility_client)

target_sources(
    qindaqt_shell_visibility_client
    PRIVATE
        src/compositor_visibility_client.cpp
        src/qt_compositor_visibility_transport.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/shell_visibility_client/compositor_visibility_client.h
            include/qindaqt/shell_visibility_client/compositor_visibility_transport.h
            include/qindaqt/shell_visibility_client/qt_compositor_visibility_transport.h
)
target_link_libraries(
    qindaqt_shell_visibility_client
    PUBLIC QindaQt::ShellVisibility Qt6::Core Qt6::DBus
)
target_compile_features(qindaqt_shell_visibility_client PUBLIC cxx_std_20)
set_target_properties(qindaqt_shell_visibility_client PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_shell_visibility_client)

install(
    TARGETS qindaqt_shell_visibility_client
    EXPORT QindaQtTargets
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

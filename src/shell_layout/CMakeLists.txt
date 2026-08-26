# SPDX-License-Identifier: LGPL-3.0-or-later

qt_add_library(qindaqt_shell_layout STATIC)
add_library(QindaQt::ShellLayout ALIAS qindaqt_shell_layout)

target_sources(
    qindaqt_shell_layout
    PRIVATE
        src/panel_layout_solver.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/shell_layout/panel_layout_solver.h
            include/qindaqt/shell_layout/panel_layout_types.h
)

target_link_libraries(qindaqt_shell_layout PUBLIC qindaqt_profiles Qt6::Core)
target_compile_features(qindaqt_shell_layout PUBLIC cxx_std_20)
set_target_properties(qindaqt_shell_layout PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_shell_layout)

install(
    TARGETS qindaqt_shell_layout
    EXPORT QindaQtTargets
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

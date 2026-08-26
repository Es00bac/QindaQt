# SPDX-License-Identifier: LGPL-3.0-or-later

qt_add_library(qindaqt_shell_visibility STATIC)
add_library(QindaQt::ShellVisibility ALIAS qindaqt_shell_visibility)

target_sources(
    qindaqt_shell_visibility
    PRIVATE
        src/panel_visibility_policy.cpp
        src/visibility_inventory_validator.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/shell_visibility/panel_visibility_policy.h
            include/qindaqt/shell_visibility/panel_visibility_types.h
)

target_link_libraries(qindaqt_shell_visibility PUBLIC qindaqt_profiles Qt6::Core)
target_compile_features(qindaqt_shell_visibility PUBLIC cxx_std_20)
set_target_properties(qindaqt_shell_visibility PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_shell_visibility)

install(
    TARGETS qindaqt_shell_visibility
    EXPORT QindaQtTargets
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

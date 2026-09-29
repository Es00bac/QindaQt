# SPDX-License-Identifier: LGPL-3.0-or-later

# Wallpaper assignments (ADR-0286): the one bounded value and strict codec for
# `appearance.wallpaperAssignments` plus the precedence rule that picks one
# output's wallpaper. The shell paints with it and Appearance Settings edits
# it, so both link this pure value instead of keeping two copies of the rules.

qt_add_library(qindaqt_wallpaper_assignments STATIC)
add_library(QindaQt::WallpaperAssignments ALIAS qindaqt_wallpaper_assignments)
target_sources(
    qindaqt_wallpaper_assignments
    PRIVATE
        src/wallpaper_assignments.cpp
        src/wallpaper_assignments_codec.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/services/wallpaper_assignments/wallpaper_assignments.h
)
# AGENT-CONTRACT: Qt Core only. No Settings1 client, Display1, workspace
# transport, filesystem, or QML: wallpaper values are data here, and the
# callers resolve them to files and identities.
target_link_libraries(qindaqt_wallpaper_assignments PUBLIC Qt6::Core)
target_compile_features(qindaqt_wallpaper_assignments PUBLIC cxx_std_20)
set_target_properties(qindaqt_wallpaper_assignments
    PROPERTIES CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON)
qindaqt_enable_warnings(qindaqt_wallpaper_assignments)

install(
    TARGETS qindaqt_wallpaper_assignments
    EXPORT QindaQtTargets
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

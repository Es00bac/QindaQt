# SPDX-License-Identifier: LGPL-3.0-or-later

# ADR-0286: the pure wallpaper-assignment value. The test reaches no bus,
# display, compositor, or user settings; it reads only the shipped schema.
qt_add_executable(qindaqt_wallpaper_assignments_tests tst_wallpaper_assignments.cpp)
target_compile_definitions(qindaqt_wallpaper_assignments_tests
    PRIVATE QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
target_link_libraries(qindaqt_wallpaper_assignments_tests
    PRIVATE QindaQt::WallpaperAssignments QindaQt::SettingsProtocol qindaqt_settings Qt6::Test)
qindaqt_enable_warnings(qindaqt_wallpaper_assignments_tests)
add_test(NAME qindaqt.services-wallpaper-assignments COMMAND qindaqt_wallpaper_assignments_tests)
set_tests_properties(qindaqt.services-wallpaper-assignments PROPERTIES
    LABELS "services;wallpaper;settings")

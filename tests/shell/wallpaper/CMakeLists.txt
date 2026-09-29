# SPDX-License-Identifier: GPL-3.0-or-later

# The shell wallpaper controller: per-output background surfaces fed by
# Settings1 (ADR-0078) and per-display / per-desktop choices with cross-fades
# (ADR-0286). One executable runs both classes so the controller sources
# compile once: the Settings1 rows use a private bus and the resident service,
# the selection rows a fake transport and a fake selection source.
set(wallpaper_runtime_dir "${CMAKE_CURRENT_LIST_DIR}/../../../src/shell/runtime")
qt_add_executable(
    qindaqt_wallpaper_controller_tests
    tst_wallpapercontroller.cpp
    tst_wallpaperselection.cpp
    "${wallpaper_runtime_dir}/wallpapercontroller.cpp"
    "${wallpaper_runtime_dir}/wallpapercontroller.h"
    "${wallpaper_runtime_dir}/wallpaperselectionsource.h"
    "${wallpaper_runtime_dir}/shellpreferencevalues.cpp"
    "${wallpaper_runtime_dir}/shellpreferencevalues.h"
    "${wallpaper_runtime_dir}/shortcutnotecontroller.cpp"
    "${wallpaper_runtime_dir}/shortcutnotecontroller.h"
    "${wallpaper_runtime_dir}/shortcutnoteshortcut.cpp"
    "${wallpaper_runtime_dir}/shortcutnoteshortcut.h"
    "${wallpaper_runtime_dir}/kglobalaccelshortcutregistrar.cpp"
    "${wallpaper_runtime_dir}/kglobalaccelshortcutregistrar.h"
)
target_include_directories(
    qindaqt_wallpaper_controller_tests
    PRIVATE "${wallpaper_runtime_dir}"
)
target_link_libraries(
    qindaqt_wallpaper_controller_tests
    PRIVATE
        QindaQt::DesignTokens
        QindaQt::SettingsClient
        QindaQt::SettingsService
        QindaQt::WallpaperAssignments
        KF6::GlobalAccel
        LayerShellQt::Interface
        Qt6::DBus
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::Test
)
target_compile_definitions(
    qindaqt_wallpaper_controller_tests
    PRIVATE
        QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
        QINDAQT_DBUS_DAEMON_EXECUTABLE="${DBUS_DAEMON_EXECUTABLE}"
)
qindaqt_enable_warnings(qindaqt_wallpaper_controller_tests)
add_test(
    NAME qindaqt.shell-wallpaper-controller
    COMMAND qindaqt_wallpaper_controller_tests
)
set_tests_properties(
    qindaqt.shell-wallpaper-controller
    PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software"
)

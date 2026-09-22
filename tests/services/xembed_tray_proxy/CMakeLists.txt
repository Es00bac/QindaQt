# SPDX-License-Identifier: GPL-3.0-or-later

# ADR-0229 end-to-end proof: dock a minimal XEmbed tray icon and see the proxy
# publish it as a StatusNotifierItem. A real Wine tray icon is exactly this —
# an X window that asks to dock — so this is the available proof without Wine.
find_package(Qt6 6.11 REQUIRED COMPONENTS Core DBus)
find_package(PkgConfig REQUIRED)
pkg_check_modules(QINDAQT_TRAY_PROBE_XCB REQUIRED IMPORTED_TARGET xcb)

qt_add_executable(qindaqt_tray_dock_probe tray_dock_probe.cpp)
target_link_libraries(
    qindaqt_tray_dock_probe
    PRIVATE Qt6::Core Qt6::DBus PkgConfig::QINDAQT_TRAY_PROBE_XCB
)
target_compile_features(qindaqt_tray_dock_probe PRIVATE cxx_std_20)
set_target_properties(qindaqt_tray_dock_probe PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_tray_dock_probe)

# AGENT-CONTRACT: the driver stands up its own Xwayland display and its own
# dbus-daemon and tears both down. It never uses the operator's display or the
# session bus, and it skips (77) rather than failing when Xwayland, a Wayland
# compositor, or dbus-daemon is unavailable — the established mechanism used by
# DesktopSessionTests and GabbeeTests.
add_test(
    NAME services.xembed-tray-dock
    COMMAND
        "${CMAKE_COMMAND}" -E env
        "QINDAQT_TRAY_PROXY=$<TARGET_FILE:qindaqt-xembed-tray-proxy>"
        "QINDAQT_TRAY_PROBE=$<TARGET_FILE:qindaqt_tray_dock_probe>"
        "QINDAQT_TRAY_WORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/dock-run"
        "${CMAKE_CURRENT_SOURCE_DIR}/run_tray_dock_proof.sh"
)
set_tests_properties(
    services.xembed-tray-dock
    PROPERTIES
        LABELS "integration;tray;x11;dbus;private-bus"
        SKIP_RETURN_CODE 77
        TIMEOUT 180
        RUN_SERIAL TRUE
)

# Regression: an orphaned proxy must not outlive its X display, and must never
# busy-loop on a dead XCB descriptor. Same private-display, private-bus
# contract as the dock proof above.
add_test(
    NAME services.xembed-tray-display-loss
    COMMAND
        "${CMAKE_COMMAND}" -E env
        "QINDAQT_TRAY_PROXY=$<TARGET_FILE:qindaqt-xembed-tray-proxy>"
        "QINDAQT_TRAY_WORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/display-loss-run"
        "${CMAKE_CURRENT_SOURCE_DIR}/run_tray_display_loss_proof.sh"
)
set_tests_properties(
    services.xembed-tray-display-loss
    PROPERTIES
        LABELS "integration;tray;x11;dbus;private-bus"
        SKIP_RETURN_CODE 77
        TIMEOUT 180
        RUN_SERIAL TRUE
)

# SPDX-License-Identifier: GPL-3.0-or-later
#
# AGENT-CONTRACT: included from tests/session/CMakeLists.txt inside the block
# that already requires QINDAQT_DBUS_RUN_SESSION, QINDAQT_KWIN_WAYLAND,
# QINDAQT_XDPYINFO and QINDAQT_XWAYLAND and defines _qindaqt_test_plugin_root.
# The nested row proves the compositor half of the windowManagement.* bridge
# (ADR-0209): a kwinrc rewrite plus org.kde.KWin.reconfigure rebinds the
# docking chord and the focus policy live.

function(qindaqt_add_docking_chord_test scenario)
    set(test_name "compositor.window-management-bridge.docking-chord.${scenario}")
    add_test(
        NAME "${test_name}"
        COMMAND
            "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_LIST_DIR}/run_docking_chord.py"
            --launcher "$<TARGET_FILE:qindaqt-wm>"
            --plugin-root "${_qindaqt_test_plugin_root}"
            --kwin "${QINDAQT_KWIN_WAYLAND}"
            --scenario "${PROJECT_SOURCE_DIR}/tests/scenarios/${scenario}.json"
            # Kept short: the private bus socket path must fit the Unix limit.
            --output-root "${CMAKE_BINARY_DIR}/wm"
    )
    set_tests_properties(
        "${test_name}"
        PROPERTIES
            TIMEOUT 480
            SKIP_RETURN_CODE 77
            RUN_SERIAL TRUE
            LABELS "integration;compositor;hybrid;window-management"
    )
endfunction()

qindaqt_add_docking_chord_test(single-1080p)

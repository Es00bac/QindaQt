# SPDX-License-Identifier: GPL-3.0-or-later
#
# AGENT-CONTRACT: included from tests/session/CMakeLists.txt inside the block
# that already requires QINDAQT_DBUS_RUN_SESSION, QINDAQT_KWIN_WAYLAND,
# QINDAQT_XDPYINFO and QINDAQT_XWAYLAND and defines _qindaqt_test_plugin_root.
# The rows prove touch edge gestures (ADR-0205) inside the private
# compositor: a finger swiped in from an edge with an action reaches the
# plugin's reserved touch edge and is announced on org.qindaqt.Compositor1,
# while the unassigned edge and an interior swipe stay silent.
# See docs/wiki/architecture/hybrid-chrome.md.

function(qindaqt_add_touch_edges_test flow scenario)
    set(test_name "compositor.touch-edges.${flow}.${scenario}")
    add_test(
        NAME "${test_name}"
        COMMAND
            "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_LIST_DIR}/run_touch_edges.py"
            --launcher "$<TARGET_FILE:qindaqt-wm>"
            --plugin-root "${_qindaqt_test_plugin_root}"
            --kwin "${QINDAQT_KWIN_WAYLAND}"
            --scenario "${PROJECT_SOURCE_DIR}/tests/scenarios/${scenario}.json"
            --flow "${flow}"
            # Kept short: the private bus socket path must fit the Unix limit.
            --output-root "${CMAKE_BINARY_DIR}/sv"
    )
    set_tests_properties(
        "${test_name}"
        PROPERTIES
            TIMEOUT 480
            SKIP_RETURN_CODE 77
            RUN_SERIAL TRUE
            LABELS "integration;compositor;touch;edges"
    )
endfunction()

qindaqt_add_touch_edges_test(edges single-1080p)
qindaqt_add_touch_edges_test(edges single-1440p-125)

# SPDX-License-Identifier: GPL-3.0-or-later
#
# AGENT-CONTRACT: included from tests/session/CMakeLists.txt inside the block
# that already requires QINDAQT_DBUS_RUN_SESSION, QINDAQT_KWIN_WAYLAND,
# QINDAQT_XDPYINFO and QINDAQT_XWAYLAND and defines _qindaqt_test_plugin_root.
# The nested rows prove iconified windows (ADR-0203) with captured private-
# compositor pixels, real wheel notches through the development input device,
# real client presses, and the hybrid diagnostics; see
# docs/wiki/architecture/hybrid-chrome.md ("Iconified windows").

function(qindaqt_add_iconify_visibility_test flow scenario)
    set(test_name "compositor.iconify-visibility.${flow}.${scenario}")
    add_test(
        NAME "${test_name}"
        COMMAND
            "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_LIST_DIR}/run_iconify_visibility.py"
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
            LABELS "integration;compositor;hybrid;iconify;pixels"
    )
endfunction()

qindaqt_add_iconify_visibility_test(basic single-1080p)
qindaqt_add_iconify_visibility_test(basic single-1440p-125)
qindaqt_add_iconify_visibility_test(dock single-1080p)

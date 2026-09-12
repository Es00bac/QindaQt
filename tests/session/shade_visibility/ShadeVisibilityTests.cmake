# SPDX-License-Identifier: GPL-3.0-or-later
#
# AGENT-CONTRACT: included from tests/session/CMakeLists.txt inside the block
# that already requires QINDAQT_DBUS_RUN_SESSION, QINDAQT_KWIN_WAYLAND,
# QINDAQT_XDPYINFO and QINDAQT_XWAYLAND and defines _qindaqt_test_plugin_root.
# The nested rows prove container roll-up with captured private-compositor
# pixels, real client presses, and focus/inventory observations; see
# docs/wiki/architecture/hybrid-chrome.md and ADR-0099.

add_test(
    NAME session.shade-visibility-unit
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/test_shade_visibility_unit.py"
)
set_tests_properties(session.shade-visibility-unit PROPERTIES LABELS "unit;session;shade")

function(qindaqt_add_shade_visibility_test flow kind scenario)
    set(test_name "compositor.shade-visibility.${flow}.${kind}.${scenario}")
    add_test(
        NAME "${test_name}"
        COMMAND
            "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_LIST_DIR}/run_shade_visibility.py"
            --launcher "$<TARGET_FILE:qindaqt-wm>"
            --plugin-root "${_qindaqt_test_plugin_root}"
            --kwin "${QINDAQT_KWIN_WAYLAND}"
            --scenario "${PROJECT_SOURCE_DIR}/tests/scenarios/${scenario}.json"
            --flow "${flow}"
            --kind "${kind}"
            # Kept short: the private bus socket path must fit the Unix limit.
            --output-root "${CMAKE_BINARY_DIR}/sv"
    )
    set_tests_properties(
        "${test_name}"
        PROPERTIES
            TIMEOUT 480
            SKIP_RETURN_CODE 77
            RUN_SERIAL TRUE
            LABELS "integration;compositor;hybrid;shade;pixels"
    )
endfunction()

foreach(kind IN ITEMS gtk-csd gtk-ssd gtk-borderless gtk-x11-csd xterm weston-terminal
                     firefox-wayland firefox-x11 electron)
    qindaqt_add_shade_visibility_test(cycles "${kind}" single-1080p)
endforeach()
qindaqt_add_shade_visibility_test(cycles gtk-csd single-1440p-125)
qindaqt_add_shade_visibility_test(cycles gtk-csd single-1080p-150)
qindaqt_add_shade_visibility_test(lifecycle gtk-csd single-1080p)

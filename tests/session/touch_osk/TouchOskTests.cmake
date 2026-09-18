# SPDX-License-Identifier: GPL-3.0-or-later
#
# AGENT-CONTRACT: included from tests/session/CMakeLists.txt inside the block
# that already requires QINDAQT_DBUS_RUN_SESSION, QINDAQT_KWIN_WAYLAND,
# QINDAQT_XDPYINFO and QINDAQT_XWAYLAND and defines _qindaqt_test_plugin_root.
# The rows prove the first-party on-screen keyboard (ADR-0204) inside the
# private compositor: KWin launches this build's qindaqt-osk as its input
# method, a finger on a GTK entry shows it, a finger on its keys types into
# the entry, a hardware key hides it, and the next touched entry brings it
# back. See docs/wiki/apps/on-screen-keyboard.md.

if(NOT TARGET qindaqt-osk)
    message(STATUS "touch-osk rows: qindaqt-osk is not built; rows not registered")
    return()
endif()

function(qindaqt_add_touch_osk_test flow scenario)
    set(test_name "compositor.touch-osk.${flow}.gtk-entry.${scenario}")
    add_test(
        NAME "${test_name}"
        COMMAND
            "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_LIST_DIR}/run_touch_osk.py"
            --launcher "$<TARGET_FILE:qindaqt-wm>"
            --plugin-root "${_qindaqt_test_plugin_root}"
            --kwin "${QINDAQT_KWIN_WAYLAND}"
            --osk "$<TARGET_FILE:qindaqt-osk>"
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
            LABELS "integration;compositor;touch;osk;pixels"
    )
endfunction()

qindaqt_add_touch_osk_test(osk single-1080p)
qindaqt_add_touch_osk_test(osk single-1440p-125)

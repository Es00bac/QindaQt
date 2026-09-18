# SPDX-License-Identifier: GPL-3.0-or-later
#
# AGENT-CONTRACT: included from tests/session/CMakeLists.txt inside the block
# that already requires QINDAQT_DBUS_RUN_SESSION, QINDAQT_KWIN_WAYLAND,
# QINDAQT_XDPYINFO and QINDAQT_XWAYLAND and defines _qindaqt_test_plugin_root.
# The rows prove fingers on container chrome (ADR-0193) with captured
# private-compositor pixels and inventory: tap, long press, title drag, and
# two-finger swipes, driven through the development input device's touch
# contacts. See docs/wiki/architecture/hybrid-chrome.md.

function(qindaqt_add_touch_chrome_test flow kind scenario)
    set(test_name "compositor.touch-chrome.${flow}.${kind}.${scenario}")
    add_test(
        NAME "${test_name}"
        COMMAND
            "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_LIST_DIR}/run_touch_chrome.py"
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
            LABELS "integration;compositor;hybrid;touch;pixels"
    )
endfunction()

qindaqt_add_touch_chrome_test(chrome gtk-csd single-1080p)
qindaqt_add_touch_chrome_test(chrome gtk-csd single-1440p-125)

# The Touchscreen switch (ADR-0205 §3): the preference travels a real
# Settings1 on the private bus and stops the seat's touch device.
if(TARGET qindaqt-settings-service)
    add_test(
        NAME "compositor.touch-chrome.enabled.gtk-csd.single-1080p"
        COMMAND
            "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_LIST_DIR}/run_touch_chrome.py"
            --launcher "$<TARGET_FILE:qindaqt-wm>"
            --plugin-root "${_qindaqt_test_plugin_root}"
            --kwin "${QINDAQT_KWIN_WAYLAND}"
            --scenario "${PROJECT_SOURCE_DIR}/tests/scenarios/single-1080p.json"
            --flow enabled
            --kind gtk-csd
            --settings-service "$<TARGET_FILE:qindaqt-settings-service>"
            --settings-schema-dir "${PROJECT_SOURCE_DIR}/data/settings"
            --output-root "${CMAKE_BINARY_DIR}/sv"
    )
    set_tests_properties(
        "compositor.touch-chrome.enabled.gtk-csd.single-1080p"
        PROPERTIES
            TIMEOUT 480
            SKIP_RETURN_CODE 77
            RUN_SERIAL TRUE
            LABELS "integration;compositor;hybrid;touch;settings"
    )
endif()

# SPDX-License-Identifier: GPL-3.0-or-later
#
# AGENT-CONTRACT: included from tests/session/CMakeLists.txt inside the block
# that already requires QINDAQT_DBUS_RUN_SESSION, QINDAQT_KWIN_WAYLAND,
# QINDAQT_XDPYINFO and QINDAQT_XWAYLAND and defines _qindaqt_test_plugin_root.
# The rows drive the live customization chord (Meta+right-click) through the
# private KWin's development seat against the production shell and judge the
# persisted profile against the Settings route's own editor host (parity);
# see docs/wiki/shell/customization-editor.md.

if(TARGET qindaqt_settings_customize)
    qt_add_executable(
        qindaqt-customize-parity-tool
        "${CMAKE_CURRENT_LIST_DIR}/customize_parity_tool.cpp"
    )
    target_link_libraries(
        qindaqt-customize-parity-tool
        PRIVATE qindaqt_settings_customize qindaqt_applets qindaqt_profiles
                QindaQt::ShellCustomizationEditor Qt6::Core
    )
    set_target_properties(qindaqt-customize-parity-tool PROPERTIES CXX_EXTENSIONS OFF)
    qindaqt_enable_warnings(qindaqt-customize-parity-tool)
endif()

if(TARGET qindaqt-shell AND TARGET qindaqt-customize-parity-tool)
    function(qindaqt_add_live_customization_test flow scenario)
        set(test_name "shell.live-customization.${flow}.${scenario}")
        add_test(
            NAME "${test_name}"
            COMMAND
                "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_LIST_DIR}/run_live_customization.py"
                --launcher "$<TARGET_FILE:qindaqt-wm>"
                --plugin-root "${_qindaqt_test_plugin_root}"
                --kwin "${QINDAQT_KWIN_WAYLAND}"
                --shell "$<TARGET_FILE:qindaqt-shell>"
                --parity-tool "$<TARGET_FILE:qindaqt-customize-parity-tool>"
                --scenario "${PROJECT_SOURCE_DIR}/tests/scenarios/${scenario}.json"
                --flow "${flow}"
                --profile-dir "${CMAKE_CURRENT_LIST_DIR}/../fixtures/live_customization_profiles"
                --profile-id "qindaqt"
                --theme-dir "${PROJECT_SOURCE_DIR}/data/themes"
                --applet-dir "${PROJECT_SOURCE_DIR}/data/applets"
                --applet-policy "${PROJECT_SOURCE_DIR}/data/applet-policy/default.json"
                # Kept short: the private bus socket path must fit the Unix limit.
                --output-root "${CMAKE_BINARY_DIR}/lc"
        )
        set_tests_properties(
            "${test_name}"
            PROPERTIES
                TIMEOUT 300
                SKIP_RETURN_CODE 77
                RUN_SERIAL TRUE
                RESOURCE_LOCK qindaqt-private-session
                LABELS "integration;shell;customize;input;pixels"
        )
        add_dependencies(qindaqt-customize-parity-tool qindaqt-shell qindaqt-wm)
    endfunction()

    qindaqt_add_live_customization_test(menu single-1080p)
    qindaqt_add_live_customization_test(editmode single-1080p)
    qindaqt_add_live_customization_test(menu single-1440p-125)
    qindaqt_add_live_customization_test(editmode single-1440p-125)
endif()

# SPDX-License-Identifier: GPL-3.0-or-later

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/panel-visibility-tmp")
qt_add_executable(
    qindaqt-panel-visibility-phase-settlement-tests
    "${CMAKE_CURRENT_SOURCE_DIR}/tst_panelvisibilityphasewaiter.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/panelvisibilityphasewaiter.cpp"
)
target_link_libraries(
    qindaqt-panel-visibility-phase-settlement-tests
    PRIVATE Qt6::Core Qt6::Test
)
set_target_properties(
    qindaqt-panel-visibility-phase-settlement-tests PROPERTIES CXX_EXTENSIONS OFF
)
qindaqt_enable_warnings(qindaqt-panel-visibility-phase-settlement-tests)
add_test(
    NAME desktop.virtual.panel-visibility.validator-unit
    COMMAND
        "${CMAKE_COMMAND}"
        "-DPYTHON=${Python3_EXECUTABLE}"
        "-DVALIDATOR=${CMAKE_CURRENT_SOURCE_DIR}/test_desktop_session_panel_visibility_unit.py"
        "-DSETTLEMENT_TEST=$<TARGET_FILE:qindaqt-panel-visibility-phase-settlement-tests>"
        "-DTMP_ROOT=${CMAKE_CURRENT_BINARY_DIR}/panel-visibility-tmp"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_panel_visibility_unit.cmake"
)
set_tests_properties(
    desktop.virtual.panel-visibility.validator-unit
    PROPERTIES LABELS "unit;session;screenshot;wayland;layer-shell;visibility"
)

if(
    TARGET qindaqt-desktop-session-probe
    AND QINDAQT_WESTON
    AND QINDAQT_WESTON_SCREENSHOOTER
)
    qt_add_executable(
        qindaqt-panel-visibility-session-probe
        "${CMAKE_CURRENT_SOURCE_DIR}/panelvisibilitysessionprobe.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/panelvisibilityphasewaiter.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/panelvisibilitysessionwindowproof.cpp"
    )
    target_link_libraries(
        qindaqt-panel-visibility-session-probe
        PRIVATE Qt6::Core Qt6::DBus Qt6::Gui
    )
    set_target_properties(
        qindaqt-panel-visibility-session-probe PROPERTIES CXX_EXTENSIONS OFF
    )
    qindaqt_enable_warnings(qindaqt-panel-visibility-session-probe)
    add_dependencies(
        qindaqt-panel-visibility-session-probe
        qindaqt-desktop-session-probe
        qindaqt_shell_launcher_qmlplugin
        qindaqt_global_menu_qmlplugin
        qindaqt_shell_clipboard_applet_runtimeplugin
    )
    install(
        FILES
            "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/panel_visibility_profiles/panel-visibility-proof.json"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/qindaqt/profiles"
        COMPONENT DesktopVirtual
    )
    # DesktopVirtualAppletModules.cmake owns the shared applet import and
    # loader closure for every nested row. Panel visibility adds only its
    # scenario-specific profile and probe dependencies here.

    # BuiltinAppletContent now imports the compiled Clipboard applet module.
    # Every private desktop row installs only DesktopVirtual, so stage that
    # shell-linked module through its owning install helper as well.
    qindaqt_install_clipboard_applet_runtime(DesktopVirtual)

    foreach(_panel_visibility_row IN ITEMS single-1080p single-wuxga)
        add_test(
            NAME "desktop.virtual.panel-visibility.${_panel_visibility_row}"
            COMMAND
                "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/test_panel_visibility_nested.py"
                --outer
                --interactive
                --scenario-id "${_panel_visibility_row}"
                --build-root "${CMAKE_BINARY_DIR}"
                --source-root "${PROJECT_SOURCE_DIR}"
                ${_qindaqt_desktop_common_arguments}
                --probe "$<TARGET_FILE:qindaqt-desktop-session-probe>"
                --visibility-probe
                "$<TARGET_FILE:qindaqt-panel-visibility-session-probe>"
                --bwrap "${QINDAQT_DESKTOP_BWRAP}"
                --python "${Python3_EXECUTABLE}"
                --dbus-daemon "${QINDAQT_DESKTOP_DBUS_DAEMON}"
                --kwin-wayland "${QINDAQT_KWIN_WAYLAND}"
                --weston "${QINDAQT_WESTON}"
                --weston-screenshooter "${QINDAQT_WESTON_SCREENSHOOTER}"
        )
        set_tests_properties(
            "desktop.virtual.panel-visibility.${_panel_visibility_row}"
            PROPERTIES
                TIMEOUT 110
                RUN_SERIAL TRUE
                RESOURCE_LOCK qindaqt-private-session
                FIXTURES_REQUIRED desktop_virtual_stage
                SKIP_RETURN_CODE 77
                LABELS "integration;session;display;wayland;layer-shell;security;input;screenshot;visibility"
        )
    endforeach()
    unset(_panel_visibility_row)
endif()

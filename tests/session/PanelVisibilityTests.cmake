# SPDX-License-Identifier: GPL-3.0-or-later

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/panel-visibility-tmp")
qt_add_executable(
    qindaqt-panel-visibility-control-channel-tests
    "${CMAKE_CURRENT_SOURCE_DIR}/tst_panelvisibilitycontrolchannel.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/panelvisibilitycontrolchannel.cpp"
)
target_link_libraries(
    qindaqt-panel-visibility-control-channel-tests PRIVATE Qt6::Core Qt6::Test
)
set_target_properties(
    qindaqt-panel-visibility-control-channel-tests PROPERTIES CXX_EXTENSIONS OFF
)
qindaqt_enable_warnings(qindaqt-panel-visibility-control-channel-tests)
add_test(
    NAME desktop.virtual.panel-visibility.control-channel-unit
    COMMAND qindaqt-panel-visibility-control-channel-tests
)
set_tests_properties(
    desktop.virtual.panel-visibility.control-channel-unit
    PROPERTIES LABELS "unit;session;wayland;visibility;fullscreen"
)
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
add_test(
    NAME desktop.virtual.panel-visibility.capture-loader-unit
    COMMAND
        "${CMAKE_COMMAND}" -E env
        "PYTHONDONTWRITEBYTECODE=1"
        "TMPDIR=${CMAKE_CURRENT_BINARY_DIR}/panel-visibility-tmp"
        "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/test_panel_visibility_capture_loader_unit.py"
)
set_tests_properties(
    desktop.virtual.panel-visibility.capture-loader-unit
    PROPERTIES LABELS "unit;session;screenshot;wayland;visibility"
)
qt_add_executable(
    qindaqt-panel-visibility-capture-loader-tests
    "${CMAKE_CURRENT_SOURCE_DIR}/tst_panelvisibilitycaptureprocess.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/panelvisibilitycaptureprocess.cpp"
)
target_link_libraries(
    qindaqt-panel-visibility-capture-loader-tests PRIVATE Qt6::Core Qt6::Test
)
set_target_properties(
    qindaqt-panel-visibility-capture-loader-tests PROPERTIES CXX_EXTENSIONS OFF
)
qindaqt_enable_warnings(qindaqt-panel-visibility-capture-loader-tests)
add_test(
    NAME desktop.virtual.panel-visibility.capture-loader-cpp-unit
    COMMAND qindaqt-panel-visibility-capture-loader-tests
)
set_tests_properties(
    desktop.virtual.panel-visibility.capture-loader-cpp-unit
    PROPERTIES LABELS "unit;session;screenshot;wayland;visibility"
)

if(
    TARGET qindaqt-desktop-session-probe
    AND QINDAQT_WESTON
    AND QINDAQT_WESTON_SCREENSHOOTER
)
    qt_add_executable(
        qindaqt-panel-visibility-session-probe
        "${CMAKE_CURRENT_SOURCE_DIR}/panelvisibilitysessionprobe.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/panelvisibilitycaptureprocess.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/panelvisibilityphasewaiter.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/panelvisibilitysessionwindowproof.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/panelvisibilitycontrolchannel.cpp"
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
        qindaqt_shell_task_list_appletplugin
        qindaqt_shell_status_notifier_applet_runtimeplugin
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

    # AGENT-NOTE: The production shell links the task-list applet library and
    # BuiltinAppletContent hosts it (Task List T3); this staging is load-bearing
    # for the nested DesktopVirtual stage exactly like the launcher and Global
    # Menu above.
    qt_query_qml_module(
        qindaqt_shell_task_list_applet
        QMLDIR _qindaqt_panel_visibility_task_list_qmldir
        TYPEINFO _qindaqt_panel_visibility_task_list_typeinfo
        QML_FILES _qindaqt_panel_visibility_task_list_qml_files
        QML_FILES_DEPLOY_PATHS _qindaqt_panel_visibility_task_list_deploy_paths
    )
    install(
        TARGETS qindaqt_shell_task_list_applet
                qindaqt_shell_task_list_appletplugin
        RUNTIME DESTINATION "${QT6_INSTALL_QML}/QindaQt/Shell/TaskList"
            COMPONENT DesktopVirtual
        LIBRARY DESTINATION "${QT6_INSTALL_QML}/QindaQt/Shell/TaskList"
            COMPONENT DesktopVirtual
        ARCHIVE DESTINATION "${QT6_INSTALL_QML}/QindaQt/Shell/TaskList"
            COMPONENT DesktopVirtual
    )
    install(
        FILES
            "${_qindaqt_panel_visibility_task_list_qmldir}"
            "${_qindaqt_panel_visibility_task_list_typeinfo}"
        DESTINATION "${QT6_INSTALL_QML}/QindaQt/Shell/TaskList"
        COMPONENT DesktopVirtual
    )
    foreach(qml_file deploy_path IN ZIP_LISTS
            _qindaqt_panel_visibility_task_list_qml_files
            _qindaqt_panel_visibility_task_list_deploy_paths)
        cmake_path(GET deploy_path PARENT_PATH deploy_directory)
        cmake_path(GET deploy_path FILENAME deploy_name)
        install(
            FILES "${qml_file}"
            DESTINATION "${QT6_INSTALL_QML}/QindaQt/Shell/TaskList/${deploy_directory}"
            RENAME "${deploy_name}"
            COMPONENT DesktopVirtual
        )
    endforeach()

    # AGENT-NOTE: BuiltinAppletContent.qml now imports
    # QindaQt.Shell.StatusNotifier, so the module stages through the shared
    # DesktopVirtualAppletModules.cmake inventory like the other hosted
    # applet modules; no panel-visibility-specific staging remains here.

    foreach(_panel_visibility_row IN ITEMS single-1080p single-wuxga)
        add_test(
            NAME "desktop.virtual.panel-visibility.${_panel_visibility_row}"
            COMMAND
                "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/test_panel_visibility_nested.py"
                --outer
                --interactive
                --scenario-id "${_panel_visibility_row}"
                --attempt-timeout-seconds 100
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

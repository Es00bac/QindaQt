# SPDX-License-Identifier: GPL-3.0-or-later

if(
    TARGET qindaqt-desktop-session-probe
    AND QINDAQT_WESTON
    AND QINDAQT_WESTON_SCREENSHOOTER
)
    qt_add_executable(
        qindaqt-panel-visibility-session-probe
        "${CMAKE_CURRENT_SOURCE_DIR}/panelvisibilitysessionprobe.cpp"
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
    )
    install(
        FILES
            "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/panel_visibility_profiles/panel-visibility-proof.json"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/qindaqt/profiles"
        COMPONENT DesktopVirtual
    )
    qt_query_qml_module(
        qindaqt_shell_launcher_qml
        QMLDIR _qindaqt_panel_visibility_launcher_qmldir
        TYPEINFO _qindaqt_panel_visibility_launcher_typeinfo
        QML_FILES _qindaqt_panel_visibility_launcher_qml_files
        QML_FILES_DEPLOY_PATHS _qindaqt_panel_visibility_launcher_deploy_paths
    )
    install(
        TARGETS qindaqt_shell_launcher_qml qindaqt_shell_launcher_qmlplugin
        RUNTIME DESTINATION "${QT6_INSTALL_QML}/QindaQt/Shell/Launcher"
            COMPONENT DesktopVirtual
        LIBRARY DESTINATION "${QT6_INSTALL_QML}/QindaQt/Shell/Launcher"
            COMPONENT DesktopVirtual
        ARCHIVE DESTINATION "${QT6_INSTALL_QML}/QindaQt/Shell/Launcher"
            COMPONENT DesktopVirtual
    )
    # qindaqt-shell links the backing QML library as compiled presentation;
    # retain the module copy above and place the same SONAME on the installed
    # executable's ordinary runtime path for this self-contained component.
    install(
        TARGETS
            qindaqt_shell_launcher_qml
            qindaqt_controls_qml
            qindaqt_tokens_qml
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        COMPONENT DesktopVirtual
    )
    install(
        TARGETS qindaqt_tokens_qml
        LIBRARY DESTINATION "Tokens"
        COMPONENT DesktopVirtual
    )
    install(
        FILES
            "${_qindaqt_panel_visibility_launcher_qmldir}"
            "${_qindaqt_panel_visibility_launcher_typeinfo}"
        DESTINATION "${QT6_INSTALL_QML}/QindaQt/Shell/Launcher"
        COMPONENT DesktopVirtual
    )
    foreach(qml_file deploy_path IN ZIP_LISTS
            _qindaqt_panel_visibility_launcher_qml_files
            _qindaqt_panel_visibility_launcher_deploy_paths)
        cmake_path(GET deploy_path PARENT_PATH deploy_directory)
        cmake_path(GET deploy_path FILENAME deploy_name)
        install(
            FILES "${qml_file}"
            DESTINATION "${QT6_INSTALL_QML}/QindaQt/Shell/Launcher/${deploy_directory}"
            RENAME "${deploy_name}"
            COMPONENT DesktopVirtual
        )
    endforeach()

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

# SPDX-License-Identifier: GPL-3.0-or-later

# This include owns only the integrated virtual-desktop rows. Existing focused
# compositor/session matrices remain independent regression boundaries.
if(TARGET qindaqt-shell AND NOT TARGET KF6::GlobalAccel)
    find_package(KF6GlobalAccel 6.0 REQUIRED CONFIG)
endif()

add_test(
    NAME desktop.virtual.sandbox-unit
    COMMAND
        "${Python3_EXECUTABLE}" -m unittest discover
        -s "${CMAKE_CURRENT_SOURCE_DIR}"
        -p "test_desktop_session_*_unit.py"
)
set_tests_properties(
    desktop.virtual.sandbox-unit
    PROPERTIES
        ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1"
        LABELS "unit;session;security;display"
)

set(
    _qindaqt_desktop_targets
    qindaqt-wm
    qindaqt-session
    qindaqt-notification-host
    qindaqt-shell
    qindaqt-settings-service
    qindaqt-audio-service
    qindaqt-settings
    qindaqt-editor
    qindaqt_app_shell
    qindaqt_tokens_qml
    qindaqt_tokens_qmlplugin
    qindaqt_controls_qml
    qindaqt_controls_qmlplugin
    qindaqt_settings_appearance_qml
    qindaqt_settings_appearance_qmlplugin
    qindaqt_settings_display_qml
    qindaqt_settings_display_qmlplugin
    qindaqt_settings_network_qml
    qindaqt_settings_network_qmlplugin
    qindaqt_settings_audio_qml
    qindaqt_settings_audio_qmlplugin
    qindaqt_settings_bluetooth_qml
    qindaqt_settings_bluetooth_qmlplugin
    qindaqt_settings_power_qml
    qindaqt_settings_power_qmlplugin
    qindaqt_settings_clipboard_qml
    qindaqt_settings_clipboard_qmlplugin
    qindaqt_compositor
    qindaqt_decoration
)
set(_qindaqt_desktop_targets_available TRUE)
foreach(_target IN LISTS _qindaqt_desktop_targets)
    if(NOT TARGET "${_target}")
        set(_qindaqt_desktop_targets_available FALSE)
    endif()
endforeach()

if(
    _qindaqt_desktop_targets_available
    AND QINDAQT_DESKTOP_BWRAP
    AND QINDAQT_DESKTOP_DBUS_DAEMON
    AND QINDAQT_KWIN_WAYLAND
    AND NOT IS_ABSOLUTE "${CMAKE_INSTALL_BINDIR}"
    AND NOT IS_ABSOLUTE "${KDE_INSTALL_PLUGINDIR}"
    AND NOT IS_ABSOLUTE "${CMAKE_INSTALL_DATADIR}"
    AND NOT IS_ABSOLUTE "${KDE_INSTALL_DBUSSERVICEDIR}"
)
    qt_add_executable(
        qindaqt-desktop-session-probe
        "${CMAKE_CURRENT_SOURCE_DIR}/desktopnotificationbinding.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/desktopnotificationbinding.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/desktopnotificationshellreadiness.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/desktopsessionprobe.cpp"
    )
    target_link_libraries(
        qindaqt-desktop-session-probe
        PRIVATE KF6::GlobalAccel Qt6::Core Qt6::DBus Qt6::Gui
    )
    set_target_properties(
        qindaqt-desktop-session-probe PROPERTIES CXX_EXTENSIONS OFF
    )
    qindaqt_enable_warnings(qindaqt-desktop-session-probe)
    add_dependencies(qindaqt-desktop-session-probe ${_qindaqt_desktop_targets})

    qt_add_executable(
        qindaqt-desktop-notification-binding-tests
        "${CMAKE_CURRENT_SOURCE_DIR}/desktopnotificationbinding.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/desktopnotificationbinding.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/tst_desktopnotificationbinding.cpp"
    )
    target_link_libraries(
        qindaqt-desktop-notification-binding-tests
        PRIVATE KF6::GlobalAccel Qt6::Core Qt6::DBus Qt6::Gui Qt6::Test
    )
    set_target_properties(
        qindaqt-desktop-notification-binding-tests
        PROPERTIES CXX_EXTENSIONS OFF
    )
    qindaqt_enable_warnings(qindaqt-desktop-notification-binding-tests)
    add_test(
        NAME desktop.virtual.notification-binding-unit
        COMMAND qindaqt-desktop-notification-binding-tests
    )
    set_tests_properties(
        desktop.virtual.notification-binding-unit
        PROPERTIES LABELS "unit;session;security;display;input"
    )

    include("${CMAKE_CURRENT_SOURCE_DIR}/DesktopNotificationShellReadinessTests.cmake")

    if(QINDAQT_DBUS_RUN_SESSION)
        add_test(
            NAME desktop.virtual.interaction-probe-cli-unit
            COMMAND
                "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/test_desktop_session_probe_cli.py"
                --probe "$<TARGET_FILE:qindaqt-desktop-session-probe>"
                --dbus-run-session "${QINDAQT_DBUS_RUN_SESSION}"
        )
        set_tests_properties(
            desktop.virtual.interaction-probe-cli-unit
            PROPERTIES
                ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1"
                LABELS "unit;session;security;display;input"
        )
    endif()

    # A dedicated test-only component makes package proof proportional to this
    # vertical slice. It duplicates no production path or target definition;
    # it stages the same artifacts and data at their real install locations.
    install(
        TARGETS
            qindaqt-wm
            qindaqt-session
            qindaqt-notification-host
            qindaqt-shell
            qindaqt-settings-service
            qindaqt-audio-service
            qindaqt-settings
            qindaqt-editor
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
        COMPONENT DesktopVirtual
    )

    # qindaqt-settings loads the Appearance, Display, and Network routes in
    # production. The DesktopVirtual component is intentionally self-contained,
    # so repeat each route's transitive QML payloads just as first-party
    # application components repeat their runtime imports. Omitting any backing library,
    # plugin, qmldir, typeinfo, or source named by qmldir makes the staged
    # desktop differ from the installed application contract.
    qt_query_qml_module(
        qindaqt_tokens_qml
        QMLDIR _qindaqt_desktop_tokens_qmldir
        TYPEINFO _qindaqt_desktop_tokens_typeinfo
    )
    qt_query_qml_module(
        qindaqt_controls_qml
        QMLDIR _qindaqt_desktop_controls_qmldir
        TYPEINFO _qindaqt_desktop_controls_typeinfo
        QML_FILES _qindaqt_desktop_controls_qml_files
        QML_FILES_DEPLOY_PATHS _qindaqt_desktop_controls_deploy_paths
    )
    install(
        TARGETS qindaqt_tokens_qml qindaqt_tokens_qmlplugin
        RUNTIME DESTINATION "${QT6_INSTALL_QML}/QindaQt/Tokens" COMPONENT DesktopVirtual
        LIBRARY DESTINATION "${QT6_INSTALL_QML}/QindaQt/Tokens" COMPONENT DesktopVirtual
        ARCHIVE DESTINATION "${QT6_INSTALL_QML}/QindaQt/Tokens" COMPONENT DesktopVirtual
    )
    install(
        FILES
            "${_qindaqt_desktop_tokens_qmldir}"
            "${_qindaqt_desktop_tokens_typeinfo}"
        DESTINATION "${QT6_INSTALL_QML}/QindaQt/Tokens"
        COMPONENT DesktopVirtual
    )
    install(
        TARGETS qindaqt_controls_qml qindaqt_controls_qmlplugin
        RUNTIME DESTINATION "${QT6_INSTALL_QML}/QindaQt/Controls" COMPONENT DesktopVirtual
        LIBRARY DESTINATION "${QT6_INSTALL_QML}/QindaQt/Controls" COMPONENT DesktopVirtual
        ARCHIVE DESTINATION "${QT6_INSTALL_QML}/QindaQt/Controls" COMPONENT DesktopVirtual
    )
    install(
        FILES
            "${_qindaqt_desktop_controls_qmldir}"
            "${_qindaqt_desktop_controls_typeinfo}"
        DESTINATION "${QT6_INSTALL_QML}/QindaQt/Controls"
        COMPONENT DesktopVirtual
    )
    foreach(qml_file deploy_path IN ZIP_LISTS
            _qindaqt_desktop_controls_qml_files
            _qindaqt_desktop_controls_deploy_paths)
        cmake_path(GET deploy_path PARENT_PATH deploy_directory)
        cmake_path(GET deploy_path FILENAME deploy_name)
        install(
            FILES "${qml_file}"
            DESTINATION "${QT6_INSTALL_QML}/QindaQt/Controls/${deploy_directory}"
            RENAME "${deploy_name}"
            COMPONENT DesktopVirtual
        )
    endforeach()
    include("${CMAKE_CURRENT_LIST_DIR}/DesktopSessionRouteStaging.cmake")
    install(
        TARGETS qindaqt_compositor
        LIBRARY DESTINATION "${KDE_INSTALL_PLUGINDIR}/kwin/plugins"
        COMPONENT DesktopVirtual
    )
    # The editor links the AppShell C++ boundary directly. Its production
    # RPATH resolves this shared library from the QML module directory, so the
    # whole-desktop component must stage the same runtime artifact.
    install(
        TARGETS qindaqt_app_shell
        LIBRARY DESTINATION "${QT6_INSTALL_QML}/QindaQt/AppShell"
        COMPONENT DesktopVirtual
    )
    install(
        TARGETS qindaqt_decoration
        LIBRARY DESTINATION "${KDE_INSTALL_PLUGINDIR}/${KDECORATION_PLUGIN_DIR}"
        COMPONENT DesktopVirtual
    )
    install(
        FILES
            "${PROJECT_BINARY_DIR}/src/services/settings_service/org.qindaqt.Settings1.service"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/dbus-1/services"
        COMPONENT DesktopVirtual
    )
    install(
        FILES
            "${PROJECT_BINARY_DIR}/src/services/audio_service/org.qindaqt.Audio1.service"
        DESTINATION "${KDE_INSTALL_DBUSSERVICEDIR}"
        COMPONENT DesktopVirtual
    )
    foreach(_data_directory IN ITEMS profiles themes applets settings)
        install(
            DIRECTORY "${PROJECT_SOURCE_DIR}/data/${_data_directory}/"
            DESTINATION "${CMAKE_INSTALL_DATADIR}/qindaqt/${_data_directory}"
            COMPONENT DesktopVirtual
        )
    endforeach()
    install(
        FILES "${PROJECT_SOURCE_DIR}/data/applet-policy/default.json"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/qindaqt/applet-policy"
        COMPONENT DesktopVirtual
    )

    set(
        _qindaqt_desktop_stage
        "${CMAKE_CURRENT_BINARY_DIR}/desktop-session-stage"
    )
    set(
        _qindaqt_desktop_plugin_relative
        "${KDE_INSTALL_PLUGINDIR}/kwin/plugins/$<TARGET_FILE_NAME:qindaqt_compositor>"
    )
    set(
        _qindaqt_desktop_decoration_relative
        "${KDE_INSTALL_PLUGINDIR}/${KDECORATION_PLUGIN_DIR}/$<TARGET_FILE_NAME:qindaqt_decoration>"
    )
    set(_qindaqt_desktop_common_arguments
        --stage-root "${_qindaqt_desktop_stage}"
        --bin-directory "${CMAKE_INSTALL_BINDIR}"
        --plugin-relative "${_qindaqt_desktop_plugin_relative}"
        --decoration-relative "${_qindaqt_desktop_decoration_relative}"
        --settings-service-directory "${CMAKE_INSTALL_DATADIR}/dbus-1/services"
        --audio-service-directory "${KDE_INSTALL_DBUSSERVICEDIR}"
    )

    add_test(
        NAME desktop.virtual.package-contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/test_desktop_session_package.py"
            --cmake "${CMAKE_COMMAND}"
            --build-root "${CMAKE_BINARY_DIR}"
            ${_qindaqt_desktop_common_arguments}
            --qml-directory "${QT6_INSTALL_QML}"
            --network-qml-library
            "$<TARGET_FILE_NAME:qindaqt_settings_network_qml>"
            --network-qml-plugin
            "$<TARGET_FILE_NAME:qindaqt_settings_network_qmlplugin>"
            --configuration "$<CONFIG>"
    )
    set_tests_properties(
        desktop.virtual.package-contract
        PROPERTIES
            TIMEOUT 150
            RUN_SERIAL TRUE
            FIXTURES_SETUP desktop_virtual_stage
            LABELS "integration;install;session;display"
    )

    add_test(
        NAME desktop.virtual.boot.1080p
        COMMAND
            "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/test_desktop_session_nested.py"
            --outer
            --build-root "${CMAKE_BINARY_DIR}"
            --source-root "${PROJECT_SOURCE_DIR}"
            ${_qindaqt_desktop_common_arguments}
            --probe "$<TARGET_FILE:qindaqt-desktop-session-probe>"
            --bwrap "${QINDAQT_DESKTOP_BWRAP}"
            --python "${Python3_EXECUTABLE}"
            --dbus-daemon "${QINDAQT_DESKTOP_DBUS_DAEMON}"
            --kwin-wayland "${QINDAQT_KWIN_WAYLAND}"
    )
    set_tests_properties(
        desktop.virtual.boot.1080p
        PROPERTIES
            TIMEOUT 70
            RUN_SERIAL TRUE
            RESOURCE_LOCK qindaqt-private-session
            FIXTURES_REQUIRED desktop_virtual_stage
            SKIP_RETURN_CODE 77
            LABELS "integration;session;display;wayland;layer-shell;security"
    )

    if(QINDAQT_WESTON AND QINDAQT_WESTON_SCREENSHOOTER)
        add_test(
            NAME desktop.virtual.interactive.1080p
            COMMAND
                "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/test_desktop_session_nested.py"
                --outer
                --interactive
                --build-root "${CMAKE_BINARY_DIR}"
                --source-root "${PROJECT_SOURCE_DIR}"
                ${_qindaqt_desktop_common_arguments}
                --probe "$<TARGET_FILE:qindaqt-desktop-session-probe>"
                --bwrap "${QINDAQT_DESKTOP_BWRAP}"
                --python "${Python3_EXECUTABLE}"
                --dbus-daemon "${QINDAQT_DESKTOP_DBUS_DAEMON}"
                --kwin-wayland "${QINDAQT_KWIN_WAYLAND}"
                --weston "${QINDAQT_WESTON}"
                --weston-screenshooter "${QINDAQT_WESTON_SCREENSHOOTER}"
        )
        set_tests_properties(
            desktop.virtual.interactive.1080p
            PROPERTIES
                TIMEOUT 90
                RUN_SERIAL TRUE
                RESOURCE_LOCK qindaqt-private-session
                FIXTURES_REQUIRED desktop_virtual_stage
                SKIP_RETURN_CODE 77
                LABELS "integration;session;display;wayland;layer-shell;security;input;screenshot"
        )

        # AGENT-CONTRACT: Keep this focused S3 set in sync with
        # desktop_session_matrix.EXECUTABLE_MATRIX_ROWS. These are real private
        # runs, not catalog validation or a claim to the complete release matrix.
        set(_qindaqt_desktop_matrix_rows
            single-wuxga
            single-1440p-125
            single-1080p-150
            dual-1080p-horizontal
        )
        foreach(_row IN LISTS _qindaqt_desktop_matrix_rows)
            set(_qindaqt_desktop_matrix_tool_arguments)
            if(_row STREQUAL "dual-1080p-horizontal")
                if(
                    NOT QINDAQT_KSCREEN_DOCTOR
                    OR NOT QINDAQT_KSCREEN_WAYLAND_BACKEND
                )
                    continue()
                endif()
                list(
                    APPEND _qindaqt_desktop_matrix_tool_arguments
                    --kscreen-doctor "${QINDAQT_KSCREEN_DOCTOR}"
                    --kscreen-wayland-backend
                    "${QINDAQT_KSCREEN_WAYLAND_BACKEND}"
                )
            endif()
            add_test(
                NAME "desktop.virtual.interactive.matrix.${_row}"
                COMMAND
                    "${Python3_EXECUTABLE}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/test_desktop_session_nested.py"
                    --outer
                    --interactive
                    --scenario-id "${_row}"
                    --build-root "${CMAKE_BINARY_DIR}"
                    --source-root "${PROJECT_SOURCE_DIR}"
                    ${_qindaqt_desktop_common_arguments}
                    --probe "$<TARGET_FILE:qindaqt-desktop-session-probe>"
                    --bwrap "${QINDAQT_DESKTOP_BWRAP}"
                    --python "${Python3_EXECUTABLE}"
                    --dbus-daemon "${QINDAQT_DESKTOP_DBUS_DAEMON}"
                    --kwin-wayland "${QINDAQT_KWIN_WAYLAND}"
                    --weston "${QINDAQT_WESTON}"
                    --weston-screenshooter "${QINDAQT_WESTON_SCREENSHOOTER}"
                    ${_qindaqt_desktop_matrix_tool_arguments}
            )
            set_tests_properties(
                "desktop.virtual.interactive.matrix.${_row}"
                PROPERTIES
                    TIMEOUT 120
                    RUN_SERIAL TRUE
                    RESOURCE_LOCK qindaqt-private-session
                    FIXTURES_REQUIRED desktop_virtual_stage
                    SKIP_RETURN_CODE 77
                    LABELS "integration;session;display;wayland;layer-shell;security;input;screenshot;matrix"
            )
        endforeach()
        unset(_qindaqt_desktop_matrix_tool_arguments)
        unset(_qindaqt_desktop_matrix_rows)
    endif()
endif()

unset(_qindaqt_desktop_targets)
unset(_qindaqt_desktop_targets_available)

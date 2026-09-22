# SPDX-License-Identifier: GPL-3.0-or-later

# AGENT-CONTRACT: BuiltinAppletContent.qml and AppletChip.qml import this
# module set. Keep staging data-driven so a DesktopVirtual install cannot
# repair one nested row while leaving another row dependent on an ambient
# build-tree import.
#
# AGENT-GUARD: every module BuiltinAppletContent imports belongs in this list
# and nowhere else. Clipboard and Task List used to be staged by
# PanelVisibilityTests.cmake instead, inside a block guarded on
# `QINDAQT_WESTON AND QINDAQT_WESTON_SCREENSHOOTER`. Those two modules have
# nothing to do with Weston, so on a machine with no Weston the DesktopVirtual
# component silently staged without them and desktop.virtual.stage-closure
# failed with "staged QML module QindaQt.Shell.ClipboardApplet has no qmldir" -
# a component-completeness defect wearing the costume of a missing optional
# dependency. Do not move staging back under a capability guard.
set(
    _qindaqt_desktop_applet_modules
    "qindaqt_shell_audio_applet_runtime|QindaQt/Shell/AudioApplet"
    "qindaqt_shell_clipboard_applet_runtime|QindaQt/Shell/ClipboardApplet"
    "qindaqt_shell_task_list_applet|QindaQt/Shell/TaskList"
    "qindaqt_shell_gather_overview_ui|QindaQt/Shell/GatherOverview"
    "qindaqt_shell_obs_applet_runtime|QindaQt/Shell/ObsApplet"
    "qindaqt_shell_bluetooth_applet_runtime|QindaQt/Shell/BluetoothApplet"
    "qindaqt_global_menu_qml|QindaQt/Shell/GlobalMenu"
    "qindaqt_shell_launcher_qml|QindaQt/Shell/Launcher"
    "qindaqt_shell_power_applet_runtime|QindaQt/Shell/PowerApplet"
    "qindaqt_shell_smart_lights_applet_runtime|QindaQt/Shell/SmartLightsApplet"
    "qindaqt_shell_voice_applet_runtime|QindaQt/Shell/VoiceApplet"
    "qindaqt_shell_status_notifier_applet_runtime|QindaQt/Shell/StatusNotifier"
    "qindaqt_shell_icons|QindaQt/Shell/Icons"
    "qindaqt_shell_desktop_controls_runtime|QindaQt/Shell/DesktopControls"
    "qindaqt_start_menu_qml|QindaQt/Shell/StartMenu"
    "qindaqt_desktop_surface_qml|QindaQt/Shell/DesktopSurface"
)

function(_qindaqt_install_desktop_applet_module descriptor)
    string(REPLACE "|" ";" fields "${descriptor}")
    list(GET fields 0 target)
    list(GET fields 1 relative_directory)
    qt_query_qml_module(
        "${target}"
        QMLDIR module_qmldir
        TYPEINFO module_typeinfo
        QML_FILES module_qml_files
        QML_FILES_DEPLOY_PATHS module_deploy_paths
    )

    get_target_property(module_type "${target}" TYPE)
    if(module_type STREQUAL "SHARED_LIBRARY")
        install(
            TARGETS "${target}" "${target}plugin"
            RUNTIME DESTINATION "${QT6_INSTALL_QML}/${relative_directory}"
                COMPONENT DesktopVirtual
            LIBRARY DESTINATION "${QT6_INSTALL_QML}/${relative_directory}"
                COMPONENT DesktopVirtual
            ARCHIVE DESTINATION "${QT6_INSTALL_QML}/${relative_directory}"
                COMPONENT DesktopVirtual
        )
    endif()
    install(
        FILES "${module_qmldir}" "${module_typeinfo}"
        DESTINATION "${QT6_INSTALL_QML}/${relative_directory}"
        COMPONENT DesktopVirtual
    )
    foreach(qml_file deploy_path IN ZIP_LISTS module_qml_files module_deploy_paths)
        cmake_path(GET deploy_path PARENT_PATH deploy_directory)
        cmake_path(GET deploy_path FILENAME deploy_name)
        install(
            FILES "${qml_file}"
            DESTINATION
                "${QT6_INSTALL_QML}/${relative_directory}/${deploy_directory}"
            RENAME "${deploy_name}"
            COMPONENT DesktopVirtual
        )
    endforeach()
endfunction()

foreach(module_descriptor IN LISTS _qindaqt_desktop_applet_modules)
    _qindaqt_install_desktop_applet_module("${module_descriptor}")
endforeach()

# qindaqt-shell links these three shared QML backings directly. The applet
# module copies above serve QML discovery; these copies serve the executable's
# relative loader path. Controls in turn needs Tokens at $ORIGIN/../Tokens.
install(
    TARGETS
        qindaqt_controls_qml
        qindaqt_global_menu_qml
        qindaqt_shell_launcher_qml
        qindaqt_start_menu_qml
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    COMPONENT DesktopVirtual
)
install(
    TARGETS qindaqt_tokens_qml
    LIBRARY DESTINATION "Tokens"
    COMPONENT DesktopVirtual
)

# AGENT-CONTRACT: first-party windows publish these exact desktop-file names.
# The private stage must carry the same application metadata as a production
# prefix so task buttons resolve Breeze icon names without ambient host apps.
install(
    FILES
        "${CMAKE_SOURCE_DIR}/src/apps/settings_center/org.qindaqt.Settings.desktop"
        "${CMAKE_SOURCE_DIR}/src/apps/text_editor/org.qindaqt.TextEditor.desktop"
        "${CMAKE_SOURCE_DIR}/src/apps/file_manager/org.qindaqt.FileManager.desktop"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/applications"
    COMPONENT DesktopVirtual
)

unset(_qindaqt_desktop_applet_modules)

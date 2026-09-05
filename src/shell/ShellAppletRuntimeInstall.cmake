# SPDX-License-Identifier: GPL-3.0-or-later

function(qindaqt_install_shell_applet_component component applet_id)
    install(
        TARGETS qindaqt-shell
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
        COMPONENT "${component}"
    )
    install(
        FILES "${PROJECT_SOURCE_DIR}/data/applets/${applet_id}.json"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/qindaqt/applets"
        COMPONENT "${component}"
    )
    install(
        FILES "${PROJECT_SOURCE_DIR}/data/profiles/qindaqt.json"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/qindaqt/profiles"
        COMPONENT "${component}"
    )
    install(
        FILES "${PROJECT_SOURCE_DIR}/data/themes/qinda-dark.json"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/qindaqt/themes"
        COMPONENT "${component}"
    )
    install(
        FILES "${PROJECT_SOURCE_DIR}/data/applet-policy/default.json"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/qindaqt/applet-policy"
        COMPONENT "${component}"
    )
    # AGENT-CONTRACT: A narrow qindaqt-shell component carries the direct QML
    # libraries at the same relative loader paths as the complete package.
    install(
        TARGETS qindaqt_controls_qml qindaqt_shell_launcher_qml
                qindaqt_global_menu_qml
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        COMPONENT "${component}"
    )
    install(
        TARGETS qindaqt_tokens_qml
        LIBRARY DESTINATION "Tokens"
        COMPONENT "${component}"
    )
endfunction()

function(qindaqt_install_shell_primary_applet_components)
    qindaqt_install_shell_applet_component(AudioAppletRuntime audio)
    qindaqt_install_shell_applet_component(BluetoothAppletRuntime bluetooth)
    qindaqt_install_shell_applet_component(ClipboardAppletRuntime clipboard)
    qindaqt_install_shell_applet_component(GlobalMenuAppletRuntime global-menu)
    qindaqt_install_shell_applet_component(LauncherAppletRuntime launcher)
    qindaqt_install_shell_applet_component(PowerAppletRuntime power)
endfunction()

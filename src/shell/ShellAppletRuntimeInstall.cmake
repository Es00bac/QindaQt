# SPDX-License-Identifier: GPL-3.0-or-later

function(qindaqt_install_shell_applet_payload component applet_id)
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
                qindaqt_global_menu_qml qindaqt_start_menu_qml
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
    # AGENT-GUARD: keep one literal component per qindaqt-shell install rule.
    # The closure probe inventories these declarations without executing
    # caller-dependent CMake code, so a parameter here would hide a stage.
    install(TARGETS qindaqt-shell RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
            COMPONENT AudioAppletRuntime)
    install(TARGETS qindaqt-shell RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
            COMPONENT BluetoothAppletRuntime)
    install(TARGETS qindaqt-shell RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
            COMPONENT ClipboardAppletRuntime)
    install(TARGETS qindaqt-shell RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
            COMPONENT GlobalMenuAppletRuntime)
    install(TARGETS qindaqt-shell RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
            COMPONENT LauncherAppletRuntime)
    install(TARGETS qindaqt-shell RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
            COMPONENT PowerAppletRuntime)

    qindaqt_install_shell_applet_payload(AudioAppletRuntime audio)
    qindaqt_install_shell_applet_payload(BluetoothAppletRuntime bluetooth)
    qindaqt_install_shell_applet_payload(ClipboardAppletRuntime clipboard)
    qindaqt_install_shell_applet_payload(GlobalMenuAppletRuntime global-menu)
    qindaqt_install_shell_applet_payload(LauncherAppletRuntime launcher)
    qindaqt_install_shell_applet_payload(PowerAppletRuntime power)
endfunction()

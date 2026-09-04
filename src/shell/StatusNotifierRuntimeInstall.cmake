# SPDX-License-Identifier: GPL-3.0-or-later

# Keep the Status Notifier's shell-carrying component cohesive and out of the
# production runtime target definition, mirroring TaskListRuntimeInstall.cmake.
# The Status Notifier applet component mirrors the PowerAppletRuntime
# precedent: the live shell, registrar inputs (manifest, profile, policy,
# theme), and the complete direct loader closure relocate together. The
# applet module itself (src/shell/status_notifier/applet) owns the
# compiled-library/QML install rules for QindaQt/Shell/StatusNotifier.
function(qindaqt_install_status_notifier_shell_runtime)
    install(
        TARGETS qindaqt-shell
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
        COMPONENT StatusNotifierAppletRuntime
    )
    install(
        FILES "${PROJECT_SOURCE_DIR}/data/applets/status-notifier.json"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/qindaqt/applets"
        COMPONENT StatusNotifierAppletRuntime
    )
    install(
        FILES "${PROJECT_SOURCE_DIR}/data/profiles/qindaqt.json"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/qindaqt/profiles"
        COMPONENT StatusNotifierAppletRuntime
    )
    install(
        FILES "${PROJECT_SOURCE_DIR}/data/themes/qinda-dark.json"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/qindaqt/themes"
        COMPONENT StatusNotifierAppletRuntime
    )
    install(
        FILES "${PROJECT_SOURCE_DIR}/data/applet-policy/default.json"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/qindaqt/applet-policy"
        COMPONENT StatusNotifierAppletRuntime
    )
    install(
        TARGETS qindaqt_controls_qml qindaqt_shell_launcher_qml
                qindaqt_global_menu_qml
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        COMPONENT StatusNotifierAppletRuntime
    )
    install(
        TARGETS qindaqt_tokens_qml
        LIBRARY DESTINATION "Tokens"
        COMPONENT StatusNotifierAppletRuntime
    )
endfunction()

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

# Shell-bearing install components need the compiled module even though the
# applet's own component rules already carry it. This additive helper mirrors
# qindaqt_install_clipboard_applet_runtime so each shell component stages the
# same QML runtime closure without hand-maintained copies.
function(qindaqt_install_status_notifier_applet_runtime component_name)
    foreach(qml_name IN ITEMS StatusNotifierApplet.qml StatusNotifierItemDelegate.qml)
        install(
            FILES "${PROJECT_SOURCE_DIR}/src/shell/status_notifier/applet/qml/${qml_name}"
            DESTINATION "${QT6_INSTALL_QML}/QindaQt/Shell/StatusNotifier/qml"
            COMPONENT "${component_name}"
        )
    endforeach()
    install(
        TARGETS qindaqt_shell_status_notifier_applet_runtime
                qindaqt_shell_status_notifier_applet_runtimeplugin
        ARCHIVE DESTINATION "${QT6_INSTALL_QML}/QindaQt/Shell/StatusNotifier"
            COMPONENT "${component_name}"
        LIBRARY DESTINATION "${QT6_INSTALL_QML}/QindaQt/Shell/StatusNotifier"
            COMPONENT "${component_name}"
        RUNTIME DESTINATION "${QT6_INSTALL_QML}/QindaQt/Shell/StatusNotifier"
            COMPONENT "${component_name}"
    )
    install(
        FILES
            "${CMAKE_BINARY_DIR}/qml/QindaQt/Shell/StatusNotifier/qmldir"
            "${CMAKE_BINARY_DIR}/qml/QindaQt/Shell/StatusNotifier/qindaqt_shell_status_notifier_applet.qmltypes"
        DESTINATION "${QT6_INSTALL_QML}/QindaQt/Shell/StatusNotifier"
        COMPONENT "${component_name}"
    )
endfunction()

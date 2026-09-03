# SPDX-License-Identifier: GPL-3.0-or-later

# Keep the Task List's shell-carrying component cohesive and out of the
# production runtime target definition. The applet module owns its own target
# install rules; this function owns the shell binary and shared runtime inputs.
function(qindaqt_install_task_list_shell_runtime)
    install(
        TARGETS qindaqt-shell
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
        COMPONENT TaskListAppletRuntime
    )
    install(
        FILES "${PROJECT_SOURCE_DIR}/data/applets/task-list.json"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/qindaqt/applets"
        COMPONENT TaskListAppletRuntime
    )
    install(
        FILES "${PROJECT_SOURCE_DIR}/data/profiles/qindaqt.json"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/qindaqt/profiles"
        COMPONENT TaskListAppletRuntime
    )
    install(
        FILES "${PROJECT_SOURCE_DIR}/data/themes/qinda-dark.json"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/qindaqt/themes"
        COMPONENT TaskListAppletRuntime
    )
    install(
        FILES "${PROJECT_SOURCE_DIR}/data/applet-policy/default.json"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/qindaqt/applet-policy"
        COMPONENT TaskListAppletRuntime
    )

    # AGENT-CONTRACT: A narrow component that stages qindaqt-shell must carry
    # each directly linked QML library plus Tokens at Controls' baked
    # $ORIGIN/../Tokens sibling path, or the shell fails before catalog lookup.
    install(
        TARGETS qindaqt_controls_qml qindaqt_shell_launcher_qml
                qindaqt_global_menu_qml
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        COMPONENT TaskListAppletRuntime
    )
    install(
        TARGETS qindaqt_tokens_qml
        LIBRARY DESTINATION "Tokens"
        COMPONENT TaskListAppletRuntime
    )
endfunction()

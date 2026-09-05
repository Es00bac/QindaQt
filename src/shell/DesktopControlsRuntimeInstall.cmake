# SPDX-License-Identifier: GPL-3.0-or-later

# Shell-bearing install components need the compiled desktop-controls module
# because BuiltinAppletContent.qml imports QindaQt.Shell.DesktopControls
# unconditionally. This additive helper mirrors
# qindaqt_install_status_notifier_applet_runtime so each shell component
# stages the same QML runtime closure without hand-maintained copies. The
# module's QML inventory is the one list declared in
# src/shell/desktop_controls/CMakeLists.txt.
function(qindaqt_install_desktop_controls_applet_runtime component_name)
    get_property(desktop_controls_qml_files GLOBAL PROPERTY
                 QINDAQT_DESKTOP_CONTROLS_QML_FILES)
    if(NOT desktop_controls_qml_files)
        message(FATAL_ERROR
            "Desktop controls QML inventory is empty; add_subdirectory(shell/desktop_controls)"
            " must run before the shell install rules")
    endif()
    foreach(qml_relative IN LISTS desktop_controls_qml_files)
        install(
            FILES "${PROJECT_SOURCE_DIR}/src/shell/desktop_controls/${qml_relative}"
            DESTINATION "${QT6_INSTALL_QML}/QindaQt/Shell/DesktopControls/qml"
            COMPONENT "${component_name}"
        )
    endforeach()
    install(
        TARGETS qindaqt_shell_desktop_controls_runtime
                qindaqt_shell_desktop_controls_runtimeplugin
        ARCHIVE DESTINATION "${QT6_INSTALL_QML}/QindaQt/Shell/DesktopControls"
            COMPONENT "${component_name}"
        LIBRARY DESTINATION "${QT6_INSTALL_QML}/QindaQt/Shell/DesktopControls"
            COMPONENT "${component_name}"
        RUNTIME DESTINATION "${QT6_INSTALL_QML}/QindaQt/Shell/DesktopControls"
            COMPONENT "${component_name}"
    )
    install(
        FILES
            "${CMAKE_BINARY_DIR}/qml/QindaQt/Shell/DesktopControls/qmldir"
            "${CMAKE_BINARY_DIR}/qml/QindaQt/Shell/DesktopControls/qindaqt_shell_desktop_controls.qmltypes"
        DESTINATION "${QT6_INSTALL_QML}/QindaQt/Shell/DesktopControls"
        COMPONENT "${component_name}"
    )
endfunction()

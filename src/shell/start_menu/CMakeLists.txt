# SPDX-License-Identifier: GPL-3.0-or-later

# Start menu applet module (ADR-0124): the worn-Luna start button and the
# two-column start panel. Pure presentation over the shell facades the panel
# dispatcher already owns (LauncherAppletController plus the DesktopControls
# sub-facades); it carries no C++ types of its own, so the QML module is the
# whole target. STRICTLY ADDITIVE: no existing applet, theme, or profile
# rendering path is modified by this module — it only renders the
# qindaqt.applets.start-menu entry point registered in
# BuiltinAppletRegistry (see docs/wiki/adr/0124-* when landed).

find_package(Qt6 6.11 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2)

set(QINDAQT_START_MENU_QML_OUTPUT_DIRECTORY
    "${CMAKE_BINARY_DIR}/qml/QindaQt/Shell/StartMenu")

qt_add_library(qindaqt_start_menu_qml SHARED)
add_library(QindaQt::ShellStartMenuQml ALIAS qindaqt_start_menu_qml)
qt_add_qml_module(
    qindaqt_start_menu_qml
    URI QindaQt.Shell.StartMenu
    VERSION 1.0
    OUTPUT_DIRECTORY "${QINDAQT_START_MENU_QML_OUTPUT_DIRECTORY}"
    TYPEINFO qindaqt_start_menu.qmltypes
    QML_FILES
        qml/StartMenuApplet.qml
        qml/StartMenuPopup.qml
        qml/StartMenuLeftColumn.qml
        qml/StartMenuRightColumn.qml
    DEPENDENCIES
        TARGET qindaqt_shell_icons
)
target_link_libraries(
    qindaqt_start_menu_qml
    PUBLIC
        QindaQt::ShellIcons
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
)
target_compile_features(qindaqt_start_menu_qml PUBLIC cxx_std_20)
set_target_properties(qindaqt_start_menu_qml PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_start_menu_qml)
# The module imports QindaQt.Controls/Tokens/Shell.Icons at runtime; the
# plugin targets must exist before any offscreen consumer loads the module
# (LauncherApplet qml-test precedent).
add_dependencies(
    qindaqt_start_menu_qml
    qindaqt_controls_qmlplugin
    qindaqt_tokens_qmlplugin
    qindaqt_shell_iconsplugin
)

# Shell-bearing install components need this module staged because
# BuiltinAppletContent.qml will import QindaQt.Shell.StartMenu unconditionally.
# Mirrors qindaqt_install_desktop_controls_applet_runtime in
# src/shell/DesktopControlsRuntimeInstall.cmake.
function(qindaqt_install_start_menu_applet_runtime component_name)
    if(NOT TARGET qindaqt_start_menu_qml)
        message(FATAL_ERROR
            "Start menu QML module is missing; add_subdirectory(shell/start_menu)"
            " must run before the shell install rules")
    endif()
    qt_query_qml_module(
        qindaqt_start_menu_qml
        QMLDIR module_qmldir
        TYPEINFO module_typeinfo
        QML_FILES module_qml_files
        QML_FILES_DEPLOY_PATHS module_deploy_paths
    )
    install(
        TARGETS qindaqt_start_menu_qml qindaqt_start_menu_qmlplugin
        ARCHIVE DESTINATION "${QT6_INSTALL_QML}/QindaQt/Shell/StartMenu"
            COMPONENT "${component_name}"
        LIBRARY DESTINATION "${QT6_INSTALL_QML}/QindaQt/Shell/StartMenu"
            COMPONENT "${component_name}"
        RUNTIME DESTINATION "${QT6_INSTALL_QML}/QindaQt/Shell/StartMenu"
            COMPONENT "${component_name}"
    )
    install(
        FILES "${module_qmldir}" "${module_typeinfo}"
        DESTINATION "${QT6_INSTALL_QML}/QindaQt/Shell/StartMenu"
        COMPONENT "${component_name}"
    )
    foreach(qml_file deploy_path IN ZIP_LISTS module_qml_files module_deploy_paths)
        cmake_path(GET deploy_path PARENT_PATH deploy_directory)
        cmake_path(GET deploy_path FILENAME deploy_name)
        install(
            FILES "${qml_file}"
            DESTINATION
                "${QT6_INSTALL_QML}/QindaQt/Shell/StartMenu/${deploy_directory}"
            RENAME "${deploy_name}"
            COMPONENT "${component_name}"
        )
    endforeach()
endfunction()

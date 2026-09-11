# SPDX-License-Identifier: GPL-3.0-or-later

# Desktop surface (ADR-0125): the per-output background-layer surface hosting
# the desktop-icons applet. The controller target owns window/layer policy and
# desktop-inventory resolution; the runtime target adds the compiled
# QindaQt.Shell.DesktopSurface 1.0 module with the QML-facing
# NewFolderController. Both stay STATIC like every other in-process audited
# applet (desktop-controls precedent). Presentation only borrows facades:
# neither target may link a service client, a D-Bus transport, KWin, or the
# shell runtime; LayerShellQt is confined to the controller's window setup.

find_package(Qt6 6.11 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2)
find_package(LayerShellQt 6.6.6 REQUIRED)

qt_add_library(qindaqt_desktop_surface STATIC)
add_library(QindaQt::ShellDesktopSurface ALIAS qindaqt_desktop_surface)
target_sources(
    qindaqt_desktop_surface
    PRIVATE
        src/desktop_surface_controller.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/shell/desktop_surface/desktop_surface_controller.h
)
target_link_libraries(
    qindaqt_desktop_surface
    PUBLIC
        QindaQt::AppletHost
        QindaQt::AppletRuntime
        qindaqt_applets
        qindaqt_profiles
        Qt6::Core
        Qt6::Gui
        Qt6::Quick
        LayerShellQt::Interface
)
target_compile_features(qindaqt_desktop_surface PUBLIC cxx_std_20)
set_target_properties(qindaqt_desktop_surface PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_desktop_surface)

set(QINDAQT_DESKTOP_SURFACE_QML_OUTPUT_DIRECTORY
    "${CMAKE_BINARY_DIR}/qml/QindaQt/Shell/DesktopSurface")

qt_add_library(qindaqt_desktop_surface_qml STATIC)
add_library(QindaQt::ShellDesktopSurfaceQml ALIAS qindaqt_desktop_surface_qml)
qt_add_qml_module(
    qindaqt_desktop_surface_qml
    URI QindaQt.Shell.DesktopSurface
    VERSION 1.0
    OUTPUT_DIRECTORY "${QINDAQT_DESKTOP_SURFACE_QML_OUTPUT_DIRECTORY}"
    TYPEINFO qindaqt_desktop_surface.qmltypes
    SOURCES
        include/qindaqt/shell/desktop_surface/new_folder_controller.h
        src/new_folder_controller.cpp
    QML_FILES
        qml/DesktopApplicationsMenu.qml
        qml/DesktopContextMenu.qml
        qml/DesktopIconsView.qml
        qml/DesktopSurface.qml
    DEPENDENCIES
        TARGET qindaqt_shell_icons
)
# The generated QML type-registration unit includes the QML_ELEMENT header by
# its file name (icons precedent); expose its directory privately.
target_include_directories(
    qindaqt_desktop_surface_qml
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include/qindaqt/shell/desktop_surface"
)
target_link_libraries(
    qindaqt_desktop_surface_qml
    PUBLIC
        QindaQt::ShellDesktopSurface
        QindaQt::ShellIcons
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
)
target_compile_features(qindaqt_desktop_surface_qml PUBLIC cxx_std_20)
set_target_properties(qindaqt_desktop_surface_qml
                      PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_desktop_surface_qml)
add_dependencies(qindaqt_desktop_surface_qml qindaqt_shell_iconsplugin)

# Shell-bearing install components need this module staged because the
# runtime instantiates QindaQt.Shell.DesktopSurface for every profile that
# declares desktop applets (ADR-0125); without it the installed shell logs
# "No module named QindaQt.Shell.DesktopSurface" and keeps plain wallpaper.
# Mirrors qindaqt_install_start_menu_applet_runtime in src/shell/start_menu.
function(qindaqt_install_desktop_surface_runtime component_name)
    if(NOT TARGET qindaqt_desktop_surface_qml)
        message(FATAL_ERROR
            "Desktop surface QML module is missing; add_subdirectory(shell/desktop_surface)"
            " must run before the shell install rules")
    endif()
    qt_query_qml_module(
        qindaqt_desktop_surface_qml
        QMLDIR module_qmldir
        TYPEINFO module_typeinfo
        QML_FILES module_qml_files
        QML_FILES_DEPLOY_PATHS module_deploy_paths
    )
    install(
        TARGETS qindaqt_desktop_surface_qml qindaqt_desktop_surface_qmlplugin
        ARCHIVE DESTINATION "${QT6_INSTALL_QML}/QindaQt/Shell/DesktopSurface"
            COMPONENT "${component_name}"
        LIBRARY DESTINATION "${QT6_INSTALL_QML}/QindaQt/Shell/DesktopSurface"
            COMPONENT "${component_name}"
        RUNTIME DESTINATION "${QT6_INSTALL_QML}/QindaQt/Shell/DesktopSurface"
            COMPONENT "${component_name}"
    )
    install(
        FILES "${module_qmldir}" "${module_typeinfo}"
        DESTINATION "${QT6_INSTALL_QML}/QindaQt/Shell/DesktopSurface"
        COMPONENT "${component_name}"
    )
    foreach(qml_file deploy_path IN ZIP_LISTS module_qml_files module_deploy_paths)
        cmake_path(GET deploy_path PARENT_PATH deploy_directory)
        cmake_path(GET deploy_path FILENAME deploy_name)
        install(
            FILES "${qml_file}"
            DESTINATION
                "${QT6_INSTALL_QML}/QindaQt/Shell/DesktopSurface/${deploy_directory}"
            RENAME "${deploy_name}"
            COMPONENT "${component_name}"
        )
    endforeach()
endfunction()

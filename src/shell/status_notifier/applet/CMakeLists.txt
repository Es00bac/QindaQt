# SPDX-License-Identifier: LGPL-3.0-or-later

# Pure presentation projection. Keep this target dependency-light; the
# separate runtime target below is the only QObject/QML composition boundary.
# Both targets stay STATIC (Clipboard/Power applet precedent): the S1
# StatusNotifier libraries ship as non-PIC static archives owned by another
# module, so embedding them in a shared applet library is impossible without
# editing that module.

include("${CMAKE_CURRENT_SOURCE_DIR}/../../IconRuntimeInstall.cmake")

qt_add_library(qindaqt_shell_status_notifier_applet STATIC)
add_library(QindaQt::ShellStatusNotifierApplet ALIAS qindaqt_shell_status_notifier_applet)

target_sources(
    qindaqt_shell_status_notifier_applet
    PRIVATE
        src/status_notifier_applet_types.cpp
        src/status_notifier_applet_model.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/shell/status_notifier/applet/status_notifier_applet_types.h
            include/qindaqt/shell/status_notifier/applet/status_notifier_applet_model.h
)

# AGENT-CONTRACT: the pure target consumes only public StatusNotifier
# foundation values plus Qt Core. It must never link or include QObject source
# seams, QML, D-Bus transports, or icon-theme internals.
target_link_libraries(
    qindaqt_shell_status_notifier_applet
    PUBLIC QindaQt::StatusNotifier Qt6::Core
)
target_compile_features(qindaqt_shell_status_notifier_applet PUBLIC cxx_std_20)
set_target_properties(qindaqt_shell_status_notifier_applet PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_shell_status_notifier_applet)

install(
    TARGETS qindaqt_shell_status_notifier_applet
    EXPORT QindaQtTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        COMPONENT StatusNotifierAppletRuntime
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
        COMPONENT StatusNotifierAppletRuntime
)

find_package(Qt6 6.11 REQUIRED COMPONENTS DBus Gui Qml Quick QuickControls2 Svg)

set(QINDAQT_STATUS_NOTIFIER_APPLET_QML_OUTPUT_DIRECTORY
    "${CMAKE_BINARY_DIR}/qml/QindaQt/Shell/StatusNotifier")

qt_add_library(qindaqt_shell_status_notifier_applet_runtime STATIC)
add_library(QindaQt::ShellStatusNotifierAppletRuntime ALIAS
            qindaqt_shell_status_notifier_applet_runtime)
target_sources(
    qindaqt_shell_status_notifier_applet_runtime
    PRIVATE
        src/status_notifier_applet_controller.cpp
        src/status_notifier_applet_menu.cpp
        src/status_notifier_monitor_adapter.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/shell/status_notifier/applet/status_notifier_source_interface.h
            include/qindaqt/shell/status_notifier/applet/status_notifier_applet_controller.h
            include/qindaqt/shell/status_notifier/applet/status_notifier_monitor_adapter.h
)
qt_add_qml_module(
    qindaqt_shell_status_notifier_applet_runtime
    URI QindaQt.Shell.StatusNotifier
    VERSION 1.0
    OUTPUT_DIRECTORY "${QINDAQT_STATUS_NOTIFIER_APPLET_QML_OUTPUT_DIRECTORY}"
    TYPEINFO qindaqt_shell_status_notifier_applet.qmltypes
    QML_FILES
        qml/StatusNotifierApplet.qml
        qml/StatusNotifierItemDelegate.qml
        qml/StatusNotifierMenu.qml
        qml/StatusNotifierMenuItem.qml
    DEPENDENCIES
        TARGET qindaqt_shell_icons
)

# AGENT-CONTRACT: the runtime target composes the pure projection with the
# injected least-authority source seam and compiled QML. The D-Bus/Gui links
# exist only for the S1 monitor adapter and icon renderer boundaries; the
# applet must never grow its own transport, Wayland, or persistence code.
target_link_libraries(
    qindaqt_shell_status_notifier_applet_runtime
    PUBLIC
        QindaQt::ShellStatusNotifierApplet
        QindaQt::StatusNotifier
        QindaQt::StatusNotifierItemClient
        QindaQt::StatusNotifierIcon
        QindaQt::ShellIcons
        Qt6::Core
        Qt6::DBus
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
)
target_compile_features(qindaqt_shell_status_notifier_applet_runtime PUBLIC cxx_std_20)
set_target_properties(qindaqt_shell_status_notifier_applet_runtime
                      PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_shell_status_notifier_applet_runtime)
add_dependencies(
    qindaqt_shell_status_notifier_applet_runtime
    qindaqt_controls_qmlplugin
    qindaqt_shell_iconsplugin
)

# Qt owns the deploy-name calculation used by qmldir and QML tooling. Query it
# once from the module target so installation cannot drift into a
# hand-maintained second list.
qt_query_qml_module(
    qindaqt_shell_status_notifier_applet_runtime
    QML_FILES qindaqt_status_notifier_applet_qml_files
    QML_FILES_DEPLOY_PATHS qindaqt_status_notifier_applet_qml_deploy_paths
)
list(LENGTH qindaqt_status_notifier_applet_qml_files qindaqt_status_notifier_applet_qml_file_count)
list(LENGTH qindaqt_status_notifier_applet_qml_deploy_paths qindaqt_status_notifier_applet_deploy_path_count)
if(NOT qindaqt_status_notifier_applet_qml_file_count EQUAL qindaqt_status_notifier_applet_deploy_path_count)
    message(FATAL_ERROR "Status notifier applet QML sources and Qt deploy paths must remain one-to-one")
endif()

foreach(qml_file deploy_path IN ZIP_LISTS
        qindaqt_status_notifier_applet_qml_files qindaqt_status_notifier_applet_qml_deploy_paths)
    cmake_path(GET deploy_path PARENT_PATH deploy_directory)
    cmake_path(GET deploy_path FILENAME deploy_name)
    # AGENT-CONTRACT: installed tooling consumes the same relative paths Qt
    # generated into qmldir; do not hand-maintain a second list.
    install(
        FILES "${qml_file}"
        DESTINATION "${QT6_INSTALL_QML}/QindaQt/Shell/StatusNotifier/${deploy_directory}"
        RENAME "${deploy_name}"
        COMPONENT StatusNotifierAppletRuntime
    )
endforeach()

# The backing archive contains both the public C++ contract and compiled QML.
# It must install beside its generated plugin archive and qmldir so a staged
# prefix keeps the complete module; the public headers install through the
# same component so an installed consumer can build against the boundary.
install(
    TARGETS
        qindaqt_shell_status_notifier_applet_runtime
        qindaqt_shell_status_notifier_applet_runtimeplugin
    EXPORT QindaQtTargets
    ARCHIVE DESTINATION "${QT6_INSTALL_QML}/QindaQt/Shell/StatusNotifier"
        COMPONENT StatusNotifierAppletRuntime
    LIBRARY DESTINATION "${QT6_INSTALL_QML}/QindaQt/Shell/StatusNotifier"
        COMPONENT StatusNotifierAppletRuntime
    RUNTIME DESTINATION "${QT6_INSTALL_QML}/QindaQt/Shell/StatusNotifier"
        COMPONENT StatusNotifierAppletRuntime
    FILE_SET HEADERS DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
        COMPONENT StatusNotifierAppletRuntime
)
install(
    FILES
        "${QINDAQT_STATUS_NOTIFIER_APPLET_QML_OUTPUT_DIRECTORY}/qmldir"
        "${QINDAQT_STATUS_NOTIFIER_APPLET_QML_OUTPUT_DIRECTORY}/qindaqt_shell_status_notifier_applet.qmltypes"
    DESTINATION "${QT6_INSTALL_QML}/QindaQt/Shell/StatusNotifier"
    COMPONENT StatusNotifierAppletRuntime
)
qindaqt_install_shell_icons_runtime(StatusNotifierAppletRuntime)

# SPDX-License-Identifier: LGPL-3.0-or-later

# Task-list T2 applet: shell-private controller over the T0 source plus the
# injected T1 authority/operation seams, and the compiled
# QindaQt.Shell.TaskList 1.0 module. The target stays STATIC (Clipboard applet
# precedent): the applet is composed in-process by the audited shell and never
# loaded as third-party code.

find_package(Qt6 6.11 REQUIRED COMPONENTS Qml Quick QuickControls2)

set(QINDAQT_TASK_LIST_APPLET_QML_OUTPUT_DIRECTORY
    "${CMAKE_BINARY_DIR}/qml/QindaQt/Shell/TaskList")

qt_add_library(qindaqt_shell_task_list_applet STATIC)
add_library(QindaQt::ShellTaskListApplet ALIAS qindaqt_shell_task_list_applet)
target_sources(
    qindaqt_shell_task_list_applet
    PRIVATE
        src/task_list_applet_types.cpp
        src/task_list_applet_projection.cpp
        src/task_list_applet_operation_bridge.cpp
        src/task_list_applet_controller.cpp
        src/task_list_applet_controller_intents.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/shell/task_list/applet/task_list_applet_types.h
            include/qindaqt/shell/task_list/applet/task_list_applet_projection.h
            include/qindaqt/shell/task_list/applet/task_list_applet_operation_port.h
            include/qindaqt/shell/task_list/applet/task_list_applet_operation_bridge.h
            include/qindaqt/shell/task_list/applet/task_list_applet_controller.h
)
qt_add_qml_module(
    qindaqt_shell_task_list_applet
    URI QindaQt.Shell.TaskList
    VERSION 1.0
    OUTPUT_DIRECTORY "${QINDAQT_TASK_LIST_APPLET_QML_OUTPUT_DIRECTORY}"
    TYPEINFO qindaqt_shell_task_list_applet.qmltypes
    QML_FILES
        qml/TaskListApplet.qml
        qml/TaskListEntryButton.qml
)

# AGENT-CONTRACT: the applet consumes the public task-list boundaries (T0
# values/source/presentation, the producer's read-only operation authority,
# the operations adapter) plus Qt QML/Quick. It must never link or include
# D-Bus, KWin, LayerShellQt, the shell runtime, the window-actions client,
# files, or persistence; the boundary probe in tests/shell/task_list enforces
# this.
target_link_libraries(
    qindaqt_shell_task_list_applet
    PUBLIC
        QindaQt::ShellTaskList
        QindaQt::ShellTaskListProducer
        QindaQt::ShellTaskListOperations
        Qt6::Core
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
)
target_compile_features(qindaqt_shell_task_list_applet PUBLIC cxx_std_20)
set_target_properties(qindaqt_shell_task_list_applet PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_shell_task_list_applet)

# The compiled module imports QindaQt.Controls/QindaQt.Tokens; their modules
# must be built before qmlcachegen runs. In the repository build this
# directory is processed before src/controls (launcher precedent), so the
# ordering hook defers to the end of the src scope; a focused standalone
# route that already added the targets takes it immediately.
function(qindaqt_task_list_applet_qml_dependencies)
    if(TARGET qindaqt_shell_task_list_applet AND TARGET qindaqt_controls_qmlplugin)
        add_dependencies(qindaqt_shell_task_list_applet qindaqt_controls_qmlplugin)
    endif()
endfunction()
if(TARGET qindaqt_controls_qmlplugin)
    qindaqt_task_list_applet_qml_dependencies()
else()
    get_filename_component(QINDAQT_TASK_LIST_APPLET_REPO_ROOT
                           "${CMAKE_CURRENT_SOURCE_DIR}/../../../.." ABSOLUTE)
    cmake_language(
        DEFER DIRECTORY "${QINDAQT_TASK_LIST_APPLET_REPO_ROOT}/src"
        CALL qindaqt_task_list_applet_qml_dependencies
    )
endif()

# Qt owns the deploy-name calculation used by qmldir and QML tooling. Query it
# once from the module target so installation cannot drift into a
# hand-maintained second list.
qt_query_qml_module(
    qindaqt_shell_task_list_applet
    QML_FILES qindaqt_task_list_applet_qml_files
    QML_FILES_DEPLOY_PATHS qindaqt_task_list_applet_qml_deploy_paths
)
list(LENGTH qindaqt_task_list_applet_qml_files qindaqt_task_list_applet_qml_file_count)
list(LENGTH qindaqt_task_list_applet_qml_deploy_paths
     qindaqt_task_list_applet_deploy_path_count)
if(NOT qindaqt_task_list_applet_qml_file_count EQUAL
   qindaqt_task_list_applet_deploy_path_count)
    message(FATAL_ERROR
            "Task-list applet QML sources and Qt deploy paths must remain one-to-one")
endif()

foreach(qml_file deploy_path IN ZIP_LISTS
        qindaqt_task_list_applet_qml_files
        qindaqt_task_list_applet_qml_deploy_paths)
    cmake_path(GET deploy_path PARENT_PATH deploy_directory)
    cmake_path(GET deploy_path FILENAME deploy_name)
    # AGENT-CONTRACT: installed tooling consumes the same relative paths Qt
    # generated into qmldir; do not hand-maintain a second list.
    install(
        FILES "${qml_file}"
        DESTINATION "${QT6_INSTALL_QML}/QindaQt/Shell/TaskList/${deploy_directory}"
        RENAME "${deploy_name}"
        COMPONENT TaskListAppletRuntime
    )
endforeach()

# The backing archive contains both the public C++ contract and compiled QML.
# It installs beside its generated plugin archive and qmldir so a staged
# prefix keeps the complete module; the public headers install through the
# same component so an installed consumer can build against the boundary.
install(
    TARGETS qindaqt_shell_task_list_applet qindaqt_shell_task_list_appletplugin
    EXPORT QindaQtTargets
    ARCHIVE DESTINATION "${QT6_INSTALL_QML}/QindaQt/Shell/TaskList"
        COMPONENT TaskListAppletRuntime
    LIBRARY DESTINATION "${QT6_INSTALL_QML}/QindaQt/Shell/TaskList"
        COMPONENT TaskListAppletRuntime
    RUNTIME DESTINATION "${QT6_INSTALL_QML}/QindaQt/Shell/TaskList"
        COMPONENT TaskListAppletRuntime
    FILE_SET HEADERS DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
        COMPONENT TaskListAppletRuntime
)
install(
    FILES
        "${QINDAQT_TASK_LIST_APPLET_QML_OUTPUT_DIRECTORY}/qmldir"
        "${QINDAQT_TASK_LIST_APPLET_QML_OUTPUT_DIRECTORY}/qindaqt_shell_task_list_applet.qmltypes"
    DESTINATION "${QT6_INSTALL_QML}/QindaQt/Shell/TaskList"
    COMPONENT TaskListAppletRuntime
)

# Install the compiled module into any component that carries qindaqt-shell.
# Public headers remain owned by the primary TaskListAppletRuntime install
# above; sibling shell components need only the runtime QML closure.
function(qindaqt_install_task_list_applet_runtime component_name)
    qt_query_qml_module(
        qindaqt_shell_task_list_applet
        QMLDIR runtime_qmldir
        TYPEINFO runtime_typeinfo
        QML_FILES runtime_qml_files
        QML_FILES_DEPLOY_PATHS runtime_deploy_paths
    )
    install(
        TARGETS qindaqt_shell_task_list_applet
                qindaqt_shell_task_list_appletplugin
        ARCHIVE DESTINATION "${QT6_INSTALL_QML}/QindaQt/Shell/TaskList"
            COMPONENT "${component_name}"
        LIBRARY DESTINATION "${QT6_INSTALL_QML}/QindaQt/Shell/TaskList"
            COMPONENT "${component_name}"
        RUNTIME DESTINATION "${QT6_INSTALL_QML}/QindaQt/Shell/TaskList"
            COMPONENT "${component_name}"
    )
    install(
        FILES
            "${runtime_qmldir}"
            "${runtime_typeinfo}"
        DESTINATION "${QT6_INSTALL_QML}/QindaQt/Shell/TaskList"
        COMPONENT "${component_name}"
    )
    foreach(qml_file deploy_path IN ZIP_LISTS
            runtime_qml_files runtime_deploy_paths)
        cmake_path(GET deploy_path PARENT_PATH deploy_directory)
        cmake_path(GET deploy_path FILENAME deploy_name)
        install(
            FILES "${qml_file}"
            DESTINATION
                "${QT6_INSTALL_QML}/QindaQt/Shell/TaskList/${deploy_directory}"
            RENAME "${deploy_name}"
            COMPONENT "${component_name}"
        )
    endforeach()
endfunction()

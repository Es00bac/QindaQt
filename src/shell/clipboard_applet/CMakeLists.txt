# SPDX-License-Identifier: LGPL-3.0-or-later

# Pure presentation projection. Keep this target dependency-light; the
# separate runtime target below is the only QObject/QML composition boundary.
# Both targets stay STATIC (Power applet precedent): the Clipboard C0 model
# ships as a non-PIC static archive owned by another module, so embedding it
# in a shared applet library is impossible without editing that module.

qt_add_library(qindaqt_shell_clipboard_applet STATIC)
add_library(QindaQt::ShellClipboardApplet ALIAS qindaqt_shell_clipboard_applet)

target_sources(
    qindaqt_shell_clipboard_applet
    PRIVATE
        src/clipboard_applet_types.cpp
        src/clipboard_applet_model.cpp
        src/clipboard_snapshot_gate.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/shell/clipboard_applet/clipboard_applet_types.h
            include/qindaqt/shell/clipboard_applet/clipboard_applet_model.h
            include/qindaqt/shell/clipboard_applet/clipboard_snapshot_gate.h
)

# AGENT-CONTRACT: the pure target consumes only public ClipboardModel values
# plus Qt Core. It must never link or include QObject client seams, QML,
# platform transports, Wayland capture/offer internals, or host clipboards.
target_link_libraries(
    qindaqt_shell_clipboard_applet
    PUBLIC QindaQt::ClipboardModel Qt6::Core
)
target_compile_features(qindaqt_shell_clipboard_applet PUBLIC cxx_std_20)
set_target_properties(qindaqt_shell_clipboard_applet PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_shell_clipboard_applet)

install(
    TARGETS qindaqt_shell_clipboard_applet
    EXPORT QindaQtTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        COMPONENT ClipboardApplet
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
        COMPONENT ClipboardApplet
)

find_package(Qt6 6.11 REQUIRED COMPONENTS Qml Quick QuickControls2)

set(QINDAQT_CLIPBOARD_APPLET_QML_OUTPUT_DIRECTORY
    "${CMAKE_BINARY_DIR}/qml/QindaQt/Shell/ClipboardApplet")

qt_add_library(qindaqt_shell_clipboard_applet_runtime STATIC)
add_library(QindaQt::ShellClipboardAppletRuntime ALIAS
            qindaqt_shell_clipboard_applet_runtime)
target_sources(
    qindaqt_shell_clipboard_applet_runtime
    PRIVATE
        src/clipboard_applet_controller.cpp
        src/clipboard_applet_controller_search.cpp
        src/clipboard_applet_controller_snapshot.cpp
        src/clipboard_model_client_adapter.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/shell/clipboard_applet/clipboard_client_interface.h
            include/qindaqt/shell/clipboard_applet/clipboard_applet_controller.h
            include/qindaqt/shell/clipboard_applet/clipboard_model_client_adapter.h
)
qt_add_qml_module(
    qindaqt_shell_clipboard_applet_runtime
    URI QindaQt.Shell.ClipboardApplet
    VERSION 1.0
    OUTPUT_DIRECTORY "${QINDAQT_CLIPBOARD_APPLET_QML_OUTPUT_DIRECTORY}"
    TYPEINFO qindaqt_shell_clipboard_applet.qmltypes
    QML_FILES
        qml/ClipboardApplet.qml
        qml/ClipboardEntryRow.qml
)

# AGENT-CONTRACT: the runtime target composes the pure projection with the
# injected least-authority client seam and compiled QML. It must never link
# or include service implementations, D-Bus, Wayland, host clipboard engines,
# files, or persistence.
target_link_libraries(
    qindaqt_shell_clipboard_applet_runtime
    PUBLIC
        QindaQt::ShellClipboardApplet
        Qt6::Core
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
)
target_compile_features(qindaqt_shell_clipboard_applet_runtime PUBLIC cxx_std_20)
set_target_properties(qindaqt_shell_clipboard_applet_runtime
                      PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_shell_clipboard_applet_runtime)
add_dependencies(
    qindaqt_shell_clipboard_applet_runtime
    qindaqt_controls_qmlplugin
)

# Qt owns the deploy-name calculation used by qmldir and QML tooling. Query it
# once from the module target so installation cannot drift into a
# hand-maintained second list.
qt_query_qml_module(
    qindaqt_shell_clipboard_applet_runtime
    QML_FILES qindaqt_clipboard_applet_qml_files
    QML_FILES_DEPLOY_PATHS qindaqt_clipboard_applet_qml_deploy_paths
)
list(LENGTH qindaqt_clipboard_applet_qml_files qindaqt_clipboard_applet_qml_file_count)
list(LENGTH qindaqt_clipboard_applet_qml_deploy_paths qindaqt_clipboard_applet_deploy_path_count)
if(NOT qindaqt_clipboard_applet_qml_file_count EQUAL qindaqt_clipboard_applet_deploy_path_count)
    message(FATAL_ERROR "Clipboard applet QML sources and Qt deploy paths must remain one-to-one")
endif()

foreach(qml_file deploy_path IN ZIP_LISTS
        qindaqt_clipboard_applet_qml_files qindaqt_clipboard_applet_qml_deploy_paths)
    cmake_path(GET deploy_path PARENT_PATH deploy_directory)
    cmake_path(GET deploy_path FILENAME deploy_name)
    # AGENT-CONTRACT: installed tooling consumes the same relative paths Qt
    # generated into qmldir; do not hand-maintain a second list.
    install(
        FILES "${qml_file}"
        DESTINATION "${QT6_INSTALL_QML}/QindaQt/Shell/ClipboardApplet/${deploy_directory}"
        RENAME "${deploy_name}"
        COMPONENT ClipboardApplet
    )
endforeach()

# The backing archive contains both the public C++ contract and compiled QML.
# It must install beside its generated plugin archive and qmldir so a staged
# prefix keeps the complete module; the public headers install through the
# same component so an installed consumer can build against the boundary.
install(
    TARGETS qindaqt_shell_clipboard_applet_runtime qindaqt_shell_clipboard_applet_runtimeplugin
    EXPORT QindaQtTargets
    ARCHIVE DESTINATION "${QT6_INSTALL_QML}/QindaQt/Shell/ClipboardApplet"
        COMPONENT ClipboardApplet
    LIBRARY DESTINATION "${QT6_INSTALL_QML}/QindaQt/Shell/ClipboardApplet"
        COMPONENT ClipboardApplet
    RUNTIME DESTINATION "${QT6_INSTALL_QML}/QindaQt/Shell/ClipboardApplet"
        COMPONENT ClipboardApplet
    FILE_SET HEADERS DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
        COMPONENT ClipboardApplet
)
install(
    FILES
        "${QINDAQT_CLIPBOARD_APPLET_QML_OUTPUT_DIRECTORY}/qmldir"
        "${QINDAQT_CLIPBOARD_APPLET_QML_OUTPUT_DIRECTORY}/qindaqt_shell_clipboard_applet.qmltypes"
    DESTINATION "${QT6_INSTALL_QML}/QindaQt/Shell/ClipboardApplet"
    COMPONENT ClipboardApplet
)

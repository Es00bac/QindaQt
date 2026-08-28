# SPDX-License-Identifier: LGPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Core Quick)

set(QINDAQT_CLIPBOARD_APPLET_QML_OUTPUT_DIRECTORY
    "${CMAKE_BINARY_DIR}/qml/QindaQt/Shell/ClipboardApplet")

# AGENT-NOTE: SHARED (not STATIC) so the installed component stays relocatable:
# the backing library, generated plugin, qmldir, and QML files install side by
# side in the QML tree and an installed consumer links the library without
# re-linking QindaQt::ClipboardModel archives. Mirrors the AppShell precedent.
qt_add_library(qindaqt_shell_clipboard_applet SHARED)
add_library(QindaQt::ShellClipboardApplet ALIAS qindaqt_shell_clipboard_applet)

target_sources(
    qindaqt_shell_clipboard_applet
    PRIVATE
        src/clipboard_applet_types.cpp
        src/clipboard_applet_model.cpp
        src/clipboard_applet_controller.cpp
        src/clipboard_model_client_adapter.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/shell/clipboard_applet/clipboard_applet_types.h
            include/qindaqt/shell/clipboard_applet/clipboard_client_interface.h
            include/qindaqt/shell/clipboard_applet/clipboard_applet_model.h
            include/qindaqt/shell/clipboard_applet/clipboard_applet_controller.h
            include/qindaqt/shell/clipboard_applet/clipboard_model_client_adapter.h
)

qt_add_qml_module(
    qindaqt_shell_clipboard_applet
    URI QindaQt.Shell.ClipboardApplet
    VERSION 1.0
    OUTPUT_DIRECTORY "${QINDAQT_CLIPBOARD_APPLET_QML_OUTPUT_DIRECTORY}"
    TYPEINFO qindaqt_shell_clipboard_applet.qmltypes
    QML_FILES
        qml/ClipboardApplet.qml
        qml/ClipboardEntryRow.qml
)

# AGENT-CONTRACT: The applet consumes only public ClipboardModel and public
# Controls/QML seams. It must never link or include platform transports,
# Wayland capture/offer internals, or host clipboard engines.
target_link_libraries(
    qindaqt_shell_clipboard_applet
    PUBLIC
        QindaQt::ClipboardModel
        QindaQt::ControlsQml
        Qt6::Core
        Qt6::Quick
)
target_compile_features(qindaqt_shell_clipboard_applet PUBLIC cxx_std_20)
set_target_properties(
    qindaqt_shell_clipboard_applet
    PROPERTIES
        CXX_EXTENSIONS OFF
        # AGENT-CONTRACT: Controls and Tokens install as sibling Qinda modules
        # two levels up (QindaQt/Controls, QindaQt/Tokens). Keep this relative
        # so a staged or relocated prefix remains runnable.
        INSTALL_RPATH "$ORIGIN/../../Controls;$ORIGIN/../../Tokens"
)
qindaqt_enable_warnings(qindaqt_shell_clipboard_applet)
add_dependencies(
    qindaqt_shell_clipboard_applet
    qindaqt_controls_qmlplugin
)

# Qt owns the deploy-name calculation used by qmldir and QML tooling. Query it
# once from the module target so installation cannot drift into a
# hand-maintained second list.
qt_query_qml_module(
    qindaqt_shell_clipboard_applet
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
    # AGENT-CONTRACT: Installed tooling consumes the same relative paths Qt
    # generated into qmldir; do not hand-maintain a second list.
    install(
        FILES "${qml_file}"
        DESTINATION "${QT6_INSTALL_QML}/QindaQt/Shell/ClipboardApplet/${deploy_directory}"
        RENAME "${deploy_name}"
        COMPONENT ClipboardApplet
    )
endforeach()

# The backing library contains both the public C++ contract and compiled QML.
# It must remain beside its generated plugin and qmldir for relocatable
# imports; the public headers install through the same component so an
# installed consumer can link the boundary.
install(
    TARGETS qindaqt_shell_clipboard_applet qindaqt_shell_clipboard_appletplugin
    EXPORT QindaQtTargets
    RUNTIME DESTINATION "${QT6_INSTALL_QML}/QindaQt/Shell/ClipboardApplet"
        COMPONENT ClipboardApplet
    LIBRARY DESTINATION "${QT6_INSTALL_QML}/QindaQt/Shell/ClipboardApplet"
        COMPONENT ClipboardApplet
    ARCHIVE DESTINATION "${QT6_INSTALL_QML}/QindaQt/Shell/ClipboardApplet"
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

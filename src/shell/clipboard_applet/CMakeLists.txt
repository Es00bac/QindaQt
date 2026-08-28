# SPDX-License-Identifier: LGPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Core Quick)

set(QINDAQT_CLIPBOARD_APPLET_QML_OUTPUT_DIRECTORY
    "${CMAKE_BINARY_DIR}/qml/QindaQt/Shell/ClipboardApplet")

qt_add_library(qindaqt_shell_clipboard_applet STATIC)
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
set_target_properties(qindaqt_shell_clipboard_applet PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_shell_clipboard_applet)

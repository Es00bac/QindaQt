# SPDX-License-Identifier: LGPL-3.0-or-later

# Pure presentation and request values. The runtime target below is the only
# Voice1 client/QML composition boundary.
qt_add_library(qindaqt_shell_voice_applet STATIC)
add_library(QindaQt::ShellVoiceApplet ALIAS qindaqt_shell_voice_applet)

target_sources(
    qindaqt_shell_voice_applet
    PRIVATE
        src/voice_applet_presentation.cpp
        src/voice_request_state.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/shell/voice_applet/voice_applet_presentation.h
            include/qindaqt/shell/voice_applet/voice_applet_types.h
            include/qindaqt/shell/voice_applet/voice_request_state.h
)

# AGENT-CONTRACT: the pure target consumes the Voice1 wire types and Qt Core
# only. It must never link a transport, a client, QML, an audio library, or an
# input method; the projection has to stay runnable from a snapshot literal.
target_link_libraries(
    qindaqt_shell_voice_applet
    PUBLIC QindaQt::VoiceProtocol Qt6::Core
)
target_compile_features(qindaqt_shell_voice_applet PUBLIC cxx_std_20)
set_target_properties(qindaqt_shell_voice_applet PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_shell_voice_applet)

find_package(Qt6 6.11 REQUIRED COMPONENTS Qml Quick QuickControls2 Svg)

qt_add_library(qindaqt_shell_voice_applet_runtime STATIC)
add_library(QindaQt::ShellVoiceAppletRuntime ALIAS qindaqt_shell_voice_applet_runtime)
target_sources(
    qindaqt_shell_voice_applet_runtime
    PRIVATE
        src/voice_applet_controller.cpp
        src/voice_service_client_adapter.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/shell/voice_applet/voice_applet_controller.h
            include/qindaqt/shell/voice_applet/voice_client_interface.h
            include/qindaqt/shell/voice_applet/voice_service_client_adapter.h
)
qt_add_qml_module(
    qindaqt_shell_voice_applet_runtime
    URI QindaQt.Shell.VoiceApplet
    VERSION 1.0
    OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/qml/QindaQt/Shell/VoiceApplet"
    QML_FILES
        qml/VoiceActionButton.qml
        qml/VoiceApplet.qml
        qml/VoiceAppletPanel.qml
        qml/VoiceLevelMeter.qml
    DEPENDENCIES
        TARGET qindaqt_shell_icons
)

# AGENT-CONTRACT: the runtime composes the pure projection with the injected
# least-authority seam and compiled QML. VoiceServiceClientAdapter is the only
# translation unit here that names the Voice1 client, and nothing in this
# target may reach a provider process, a microphone, or the filesystem.
target_link_libraries(
    qindaqt_shell_voice_applet_runtime
    PUBLIC
        QindaQt::ShellIcons
        QindaQt::ShellVoiceApplet
        QindaQt::VoiceClient
        Qt6::Core
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
)
target_compile_features(qindaqt_shell_voice_applet_runtime PUBLIC cxx_std_20)
set_target_properties(qindaqt_shell_voice_applet_runtime PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_shell_voice_applet_runtime)

# The compiled module imports QindaQt.Controls; that module must exist before
# qmlcachegen runs. src/controls is processed before this directory (Bluetooth
# applet precedent), so the target already exists here.
add_dependencies(qindaqt_shell_voice_applet_runtime qindaqt_controls_qmlplugin)

install(
    TARGETS qindaqt_shell_voice_applet qindaqt_shell_voice_applet_runtime
    EXPORT QindaQtTargets
    FILE_SET HEADERS DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

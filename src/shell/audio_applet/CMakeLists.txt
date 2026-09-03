# SPDX-License-Identifier: GPL-3.0-or-later

# AGENT-NOTE: The root source registry adds this module only after AudioClient
# exists. The pure target is the bounded projection over public Audio1
# protocol values; the runtime target below is the only AudioClient/QML
# composition boundary.
find_package(Qt6 6.11 REQUIRED COMPONENTS Core Quick)

qt_add_library(qindaqt_shell_audio_applet STATIC)
add_library(QindaQt::ShellAudioApplet ALIAS qindaqt_shell_audio_applet)

target_sources(
    qindaqt_shell_audio_applet
    PRIVATE
        audio_applet_model.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS .
        FILES
            audio_applet_model.h
)

# AGENT-CONTRACT: The pure projection consumes only public Audio1 protocol
# values. It must never link the client, transport, QML, or the resident
# audio service; grant policy lives exclusively in the runtime controller.
target_link_libraries(
    qindaqt_shell_audio_applet
    PUBLIC
        QindaQt::AudioProtocol
        Qt6::Core
)
target_compile_features(qindaqt_shell_audio_applet PUBLIC cxx_std_20)
set_target_properties(qindaqt_shell_audio_applet PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_shell_audio_applet)

qt_add_library(qindaqt_shell_audio_applet_runtime STATIC)
add_library(QindaQt::ShellAudioAppletRuntime ALIAS
            qindaqt_shell_audio_applet_runtime)

set(QINDAQT_AUDIO_APPLET_QML_OUTPUT_DIRECTORY
    "${CMAKE_BINARY_DIR}/qml/QindaQt/Shell/AudioApplet")

target_sources(
    qindaqt_shell_audio_applet_runtime
    PRIVATE
        audio_applet_controller.cpp
        audio_applet_controller.h
)

qt_add_qml_module(
    qindaqt_shell_audio_applet_runtime
    URI QindaQt.Shell.AudioApplet
    VERSION 1.0
    OUTPUT_DIRECTORY "${QINDAQT_AUDIO_APPLET_QML_OUTPUT_DIRECTORY}"
    QML_FILES
        qml/AudioApplet.qml
        qml/AudioDeviceRow.qml
        qml/AudioStreamRow.qml
)
target_include_directories(
    qindaqt_shell_audio_applet_runtime PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}"
)

# AGENT-CONTRACT: The applet consumes only public Audio1 seams (protocol types
# plus the AudioClient facade). It must never link audio_service internals,
# WirePlumber, or GLib; QML receives only the injected controller object.
target_link_libraries(
    qindaqt_shell_audio_applet_runtime
    PUBLIC
        QindaQt::AudioClient
        QindaQt::ControlsQml
        QindaQt::ShellAudioApplet
        Qt6::Core
        Qt6::Quick
)
target_compile_features(qindaqt_shell_audio_applet_runtime PUBLIC cxx_std_20)
set_target_properties(qindaqt_shell_audio_applet_runtime
                      PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_shell_audio_applet_runtime)

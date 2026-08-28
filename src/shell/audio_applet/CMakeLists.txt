# SPDX-License-Identifier: GPL-3.0-or-later

# AGENT-NOTE: This file is the Audio applet A1 standalone build seam. It is
# deliberately not referenced by src/shell/CMakeLists.txt; the manager wires
# it in as an additive `add_subdirectory(src/shell/audio_applet)` when the
# candidate integrates. Everything here stays self-contained so that wiring
# step cannot change behavior of unrelated shell targets.
find_package(Qt6 6.11 REQUIRED COMPONENTS Core Quick)

qt_add_library(qindaqt_shell_audio_applet STATIC)
add_library(QindaQt::ShellAudioApplet ALIAS qindaqt_shell_audio_applet)

target_sources(
    qindaqt_shell_audio_applet
    PRIVATE
        audio_applet_model.cpp
        audio_applet_controller.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS .
        FILES
            audio_applet_model.h
            audio_applet_controller.h
)

qt_add_qml_module(
    qindaqt_shell_audio_applet
    URI QindaQt.Shell.AudioApplet
    VERSION 1.0
    QML_FILES
        qml/AudioApplet.qml
        qml/AudioDeviceRow.qml
        qml/AudioStreamRow.qml
)

# AGENT-CONTRACT: The applet consumes only public Audio1 seams (protocol types
# plus the AudioClient facade). It must never link audio_service internals,
# WirePlumber, or GLib; QML receives only the injected controller object.
target_link_libraries(
    qindaqt_shell_audio_applet
    PUBLIC
        QindaQt::AudioClient
        QindaQt::ControlsQml
        Qt6::Core
        Qt6::Quick
)
target_compile_features(qindaqt_shell_audio_applet PUBLIC cxx_std_20)
set_target_properties(qindaqt_shell_audio_applet PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_shell_audio_applet)

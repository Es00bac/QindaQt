# SPDX-License-Identifier: GPL-3.0-or-later

find_package(QindaTK CONFIG REQUIRED)

qt_add_library(qindaqt_voice_console_support STATIC
    voice_console_model.cpp voice_console_model.h voice_console_history.cpp)
target_include_directories(qindaqt_voice_console_support
                           PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

# AGENT-CONTRACT: the console's model is the second Voice1 consumer, after the
# panel applet. It links the public client only: no shell, no applet, no
# provider, and no D-Bus of its own — main.cpp owns the one transport.
target_link_libraries(qindaqt_voice_console_support
    PUBLIC QindaQt::VoiceClient Qt6::Core Qt6::Gui)
target_compile_features(qindaqt_voice_console_support PUBLIC cxx_std_20)
set_target_properties(qindaqt_voice_console_support PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_voice_console_support)

qt_add_executable(qindaqt-voice main.cpp)
qt_add_qml_module(qindaqt-voice
    URI QindaQt.VoiceConsole VERSION 1.0 RESOURCE_PREFIX /qt/qml
    QML_FILES qml/Main.qml)
target_link_libraries(qindaqt-voice PRIVATE qindaqt_voice_console_support
    QindaTK::qindatk Qt6::Qml Qt6::Quick Qt6::QuickControls2 Qt6::DBus)
qindaqt_enable_warnings(qindaqt-voice)

# AGENT-CONTRACT: the Controls and Tokens backing libraries are installed in
# the QML tree, outside the system library path. Preserve relocation into a
# staged prefix the same way the Viewer does.
set(voice_bindir "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}")
set(voice_qmldir "${QT6_INSTALL_QML}")
cmake_path(ABSOLUTE_PATH voice_qmldir BASE_DIRECTORY "${CMAKE_INSTALL_PREFIX}" NORMALIZE)
cmake_path(RELATIVE_PATH voice_qmldir BASE_DIRECTORY "${voice_bindir}"
    OUTPUT_VARIABLE voice_qml_relative)
set_target_properties(qindaqt-voice PROPERTIES
    INSTALL_RPATH "$ORIGIN/${voice_qml_relative}/QindaQt/Tokens;$ORIGIN/${voice_qml_relative}/QindaQt/Controls")

install(TARGETS qindaqt-voice RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
        COMPONENT VoiceConsole)
install(FILES org.qindaqt.Voice.desktop
    DESTINATION "${CMAKE_INSTALL_DATADIR}/applications" COMPONENT VoiceConsole)
install(FILES qindaqt-voice.svg
    DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps"
    COMPONENT VoiceConsole)

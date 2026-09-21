# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_library(qindaqt_settings_voice STATIC)
target_sources(
    qindaqt_settings_voice
    PRIVATE
        voice_settings_model.cpp
        voice_settings_provider.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/apps/settings_voice/voice_settings_model.h
)
target_link_libraries(
    qindaqt_settings_voice
    PUBLIC QindaQt::VoiceClient QindaQt::SettingsClient Qt6::Core
)
target_compile_features(qindaqt_settings_voice PUBLIC cxx_std_20)
set_target_properties(
    qindaqt_settings_voice
    PROPERTIES CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON
)
qindaqt_enable_warnings(qindaqt_settings_voice)

set(QINDAQT_VOICE_SETTINGS_QML_OUTPUT_DIRECTORY
    "${CMAKE_BINARY_DIR}/qml/QindaQt/SettingsApp/Voice")
qt_add_library(qindaqt_settings_voice_qml STATIC)
qt_add_qml_module(
    qindaqt_settings_voice_qml
    URI QindaQt.SettingsApp.Voice
    VERSION 1.0
    CLASS_NAME QindaQtSettingsVoicePlugin
    OUTPUT_DIRECTORY "${QINDAQT_VOICE_SETTINGS_QML_OUTPUT_DIRECTORY}"
    TYPEINFO qindaqt_settings_voice.qmltypes
    SOURCES
        voice_route_composition.h
        voice_route_composition.cpp
    QML_FILES qml/VoicePage.qml
)
target_link_libraries(
    qindaqt_settings_voice_qml
    PRIVATE
        qindaqt_settings_voice QindaQt::VoiceClient
        QindaQt::SettingsClient QindaQt::TokensQml
        Qt6::Core Qt6::DBus Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2
)
set_target_properties(
    qindaqt_settings_voice_qml PROPERTIES
    CXX_EXTENSIONS OFF
    INSTALL_RPATH "$ORIGIN/../../Tokens;$ORIGIN/../../Controls"
)
qindaqt_enable_warnings(qindaqt_settings_voice_qml)

install(
    TARGETS qindaqt_settings_voice_qml qindaqt_settings_voice_qmlplugin
    RUNTIME DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Voice"
        COMPONENT SettingsAppearanceRuntime
    LIBRARY DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Voice"
        COMPONENT SettingsAppearanceRuntime
    ARCHIVE DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Voice"
        COMPONENT SettingsAppearanceRuntime
)
install(
    FILES
        "${QINDAQT_VOICE_SETTINGS_QML_OUTPUT_DIRECTORY}/qmldir"
        "${QINDAQT_VOICE_SETTINGS_QML_OUTPUT_DIRECTORY}/qindaqt_settings_voice.qmltypes"
        qml/VoicePage.qml
    DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Voice"
    COMPONENT SettingsAppearanceRuntime
)

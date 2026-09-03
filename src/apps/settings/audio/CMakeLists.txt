# SPDX-License-Identifier: GPL-3.0-or-later

# Public-Audio1 consumer model for the first-party Settings Audio route.
qt_add_library(qindaqt_settings_audio STATIC)
target_sources(
    qindaqt_settings_audio
    PRIVATE
        audio_settings_model.cpp
        audio_settings_projection.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/apps/settings_audio/audio_settings_model.h
)
target_link_libraries(
    qindaqt_settings_audio
    PUBLIC QindaQt::AudioClient QindaQt::AudioProtocol Qt6::Core
)
target_compile_features(qindaqt_settings_audio PUBLIC cxx_std_20)
set_target_properties(
    qindaqt_settings_audio
    PROPERTIES CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON
)
qindaqt_enable_warnings(qindaqt_settings_audio)

set(QINDAQT_AUDIO_SETTINGS_QML_OUTPUT_DIRECTORY
    "${CMAKE_BINARY_DIR}/qml/QindaQt/SettingsApp/Audio")
qt_add_library(qindaqt_settings_audio_qml SHARED)
qt_add_qml_module(
    qindaqt_settings_audio_qml
    URI QindaQt.SettingsApp.Audio
    VERSION 1.0
    CLASS_NAME QindaQtSettingsAudioPlugin
    OUTPUT_DIRECTORY "${QINDAQT_AUDIO_SETTINGS_QML_OUTPUT_DIRECTORY}"
    TYPEINFO qindaqt_settings_audio.qmltypes
    QML_FILES
        qml/AudioDeviceSection.qml
        qml/AudioLevelRow.qml
        qml/AudioPage.qml
        qml/AudioStreamSection.qml
)
target_link_libraries(
    qindaqt_settings_audio_qml
    PRIVATE QindaQt::TokensQml Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick
            Qt6::QuickControls2
)
set_target_properties(
    qindaqt_settings_audio_qml
    PROPERTIES
    CXX_EXTENSIONS OFF
    INSTALL_RPATH "$ORIGIN/../../Tokens;$ORIGIN/../../Controls"
)
qindaqt_enable_warnings(qindaqt_settings_audio_qml)

install(
    TARGETS qindaqt_settings_audio_qml qindaqt_settings_audio_qmlplugin
    RUNTIME DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Audio"
        COMPONENT SettingsAppearanceRuntime
    LIBRARY DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Audio"
        COMPONENT SettingsAppearanceRuntime
    ARCHIVE DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Audio"
        COMPONENT SettingsAppearanceRuntime
)
install(
    FILES
        "${QINDAQT_AUDIO_SETTINGS_QML_OUTPUT_DIRECTORY}/qmldir"
        "${QINDAQT_AUDIO_SETTINGS_QML_OUTPUT_DIRECTORY}/qindaqt_settings_audio.qmltypes"
    DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Audio"
    COMPONENT SettingsAppearanceRuntime
)
install(
    FILES
        qml/AudioDeviceSection.qml
        qml/AudioLevelRow.qml
        qml/AudioPage.qml
        qml/AudioStreamSection.qml
    DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Audio"
    COMPONENT SettingsAppearanceRuntime
)

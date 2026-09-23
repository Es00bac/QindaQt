# SPDX-License-Identifier: GPL-3.0-or-later

# The Settings Streaming route: presentation and sequencing over the shared
# obs-websocket client, the scoped keyring store, and the provisioning
# functions. It writes no OBS file itself.
qt_add_library(qindaqt_settings_streaming STATIC)
target_sources(
    qindaqt_settings_streaming
    PRIVATE
        streaming_settings_model.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/apps/settings_streaming/settings1_streaming_preferences.h
            include/qindaqt/apps/settings_streaming/streaming_settings_model.h
)
target_link_libraries(
    qindaqt_settings_streaming
    PUBLIC QindaQt::ObsClient QindaQt::StreamingPreferences QindaQt::SessionAutostart Qt6::Core Qt6::Gui
)
target_compile_features(qindaqt_settings_streaming PUBLIC cxx_std_20)
set_target_properties(
    qindaqt_settings_streaming PROPERTIES
    CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON
)
qindaqt_enable_warnings(qindaqt_settings_streaming)

# Backend composition singleton, as every other route does it.
set(QINDAQT_STREAMING_BACKEND_QML_OUTPUT_DIRECTORY
    "${CMAKE_BINARY_DIR}/qml/QindaQt/SettingsApp/StreamingBackend")
qt_add_library(qindaqt_settings_streaming_backend_qml STATIC)
qt_add_qml_module(
    qindaqt_settings_streaming_backend_qml
    URI QindaQt.SettingsApp.StreamingBackend
    VERSION 1.0
    CLASS_NAME QindaQtSettingsStreamingBackendPlugin
    OUTPUT_DIRECTORY "${QINDAQT_STREAMING_BACKEND_QML_OUTPUT_DIRECTORY}"
    SOURCES
        streaming_route_composition.h
        streaming_route_composition.cpp
)
target_link_libraries(
    qindaqt_settings_streaming_backend_qml
    PRIVATE qindaqt_settings_streaming QindaQt::TokensQml
            Qt6::Core Qt6::DBus Qt6::Gui Qt6::Qml Qt6::Quick
)
set_target_properties(qindaqt_settings_streaming_backend_qml PROPERTIES
    CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON)
qindaqt_enable_warnings(qindaqt_settings_streaming_backend_qml)

# Route QML pages.
set(QINDAQT_STREAMING_QML_OUTPUT_DIRECTORY
    "${CMAKE_BINARY_DIR}/qml/QindaQt/SettingsApp/Streaming")
qt_add_library(qindaqt_settings_streaming_qml SHARED)
qt_add_qml_module(
    qindaqt_settings_streaming_qml
    URI QindaQt.SettingsApp.Streaming
    VERSION 1.0
    CLASS_NAME QindaQtSettingsStreamingPlugin
    OUTPUT_DIRECTORY "${QINDAQT_STREAMING_QML_OUTPUT_DIRECTORY}"
    TYPEINFO qindaqt_settings_streaming.qmltypes
    QML_FILES
        qml/StreamingBusMappingSection.qml
        qml/StreamingOutputsSection.qml
        qml/StreamingPage.qml
)
target_link_libraries(
    qindaqt_settings_streaming_qml
    PRIVATE QindaQt::TokensQml
            Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2
)
set_target_properties(
    qindaqt_settings_streaming_qml PROPERTIES
    CXX_EXTENSIONS OFF
    INSTALL_RPATH "$ORIGIN/../../Tokens;$ORIGIN/../../Controls"
)
qindaqt_enable_warnings(qindaqt_settings_streaming_qml)

# The route ships inside the shared SettingsAppearanceRuntime component like
# every other Settings route; a separate component would leave Streaming out
# of the relocated product.
install(
    TARGETS qindaqt_settings_streaming_qml qindaqt_settings_streaming_qmlplugin
    RUNTIME DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Streaming"
        COMPONENT SettingsAppearanceRuntime
    LIBRARY DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Streaming"
        COMPONENT SettingsAppearanceRuntime
    ARCHIVE DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Streaming"
        COMPONENT SettingsAppearanceRuntime
)
install(
    FILES
        "${QINDAQT_STREAMING_QML_OUTPUT_DIRECTORY}/qmldir"
        "${QINDAQT_STREAMING_QML_OUTPUT_DIRECTORY}/qindaqt_settings_streaming.qmltypes"
        qml/StreamingBusMappingSection.qml
        qml/StreamingOutputsSection.qml
        qml/StreamingPage.qml
    DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Streaming"
    COMPONENT SettingsAppearanceRuntime
)

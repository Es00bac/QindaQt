# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_library(qindaqt_settings_clipboard STATIC)
target_sources(
    qindaqt_settings_clipboard
    PRIVATE
        clipboard_settings_model.cpp
        clipboard_settings_service.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/apps/settings_clipboard/clipboard_settings_model.h
)
target_link_libraries(
    qindaqt_settings_clipboard
    PUBLIC QindaQt::ClipboardClient QindaQt::SettingsClient Qt6::Core
)
target_compile_features(qindaqt_settings_clipboard PUBLIC cxx_std_20)
set_target_properties(
    qindaqt_settings_clipboard
    PROPERTIES CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON
)
qindaqt_enable_warnings(qindaqt_settings_clipboard)

set(QINDAQT_CLIPBOARD_SETTINGS_QML_OUTPUT_DIRECTORY
    "${CMAKE_BINARY_DIR}/qml/QindaQt/SettingsApp/Clipboard")
qt_add_library(qindaqt_settings_clipboard_qml STATIC)
qt_add_qml_module(
    qindaqt_settings_clipboard_qml
    URI QindaQt.SettingsApp.Clipboard
    VERSION 1.0
    CLASS_NAME QindaQtSettingsClipboardPlugin
    OUTPUT_DIRECTORY "${QINDAQT_CLIPBOARD_SETTINGS_QML_OUTPUT_DIRECTORY}"
    TYPEINFO qindaqt_settings_clipboard.qmltypes
    SOURCES
        clipboard_route_composition.h
        clipboard_route_composition.cpp
    QML_FILES qml/ClipboardPage.qml
)
target_link_libraries(
    qindaqt_settings_clipboard_qml
    PRIVATE
        qindaqt_settings_clipboard QindaQt::ClipboardClient
        QindaQt::SettingsClient QindaQt::TokensQml
        Qt6::Core Qt6::DBus Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2
)
set_target_properties(
    qindaqt_settings_clipboard_qml PROPERTIES
    CXX_EXTENSIONS OFF
    INSTALL_RPATH "$ORIGIN/../../Tokens;$ORIGIN/../../Controls"
)
qindaqt_enable_warnings(qindaqt_settings_clipboard_qml)

install(
    TARGETS qindaqt_settings_clipboard_qml qindaqt_settings_clipboard_qmlplugin
    RUNTIME DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Clipboard"
        COMPONENT SettingsAppearanceRuntime
    LIBRARY DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Clipboard"
        COMPONENT SettingsAppearanceRuntime
    ARCHIVE DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Clipboard"
        COMPONENT SettingsAppearanceRuntime
)
install(
    FILES
        "${QINDAQT_CLIPBOARD_SETTINGS_QML_OUTPUT_DIRECTORY}/qmldir"
        "${QINDAQT_CLIPBOARD_SETTINGS_QML_OUTPUT_DIRECTORY}/qindaqt_settings_clipboard.qmltypes"
        qml/ClipboardPage.qml
    DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Clipboard"
    COMPONENT SettingsAppearanceRuntime
)

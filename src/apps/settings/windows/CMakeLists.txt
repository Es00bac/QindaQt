# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_library(qindaqt_settings_windows STATIC)
target_sources(
    qindaqt_settings_windows
    PRIVATE
        windows_settings_model.cpp
        windows_values.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/apps/settings_windows/windows_settings_model.h
            include/qindaqt/apps/settings_windows/windows_values.h
)
target_link_libraries(
    qindaqt_settings_windows
    PUBLIC QindaQt::SettingsClient Qt6::Core
)
target_compile_features(qindaqt_settings_windows PUBLIC cxx_std_20)
set_target_properties(
    qindaqt_settings_windows
    PROPERTIES CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON
)
qindaqt_enable_warnings(qindaqt_settings_windows)

set(QINDAQT_WINDOWS_SETTINGS_QML_OUTPUT_DIRECTORY
    "${CMAKE_BINARY_DIR}/qml/QindaQt/SettingsApp/Windows")
qt_add_library(qindaqt_settings_windows_qml STATIC)
qt_add_qml_module(
    qindaqt_settings_windows_qml
    URI QindaQt.SettingsApp.Windows
    VERSION 1.0
    CLASS_NAME QindaQtSettingsWindowsPlugin
    OUTPUT_DIRECTORY "${QINDAQT_WINDOWS_SETTINGS_QML_OUTPUT_DIRECTORY}"
    TYPEINFO qindaqt_settings_windows.qmltypes
    SOURCES
        windows_session_apply_status_client.h
        windows_session_apply_status_client.cpp
        windows_route_composition.h
        windows_route_composition.cpp
    QML_FILES qml/WindowsPage.qml qml/WindowsChoiceRow.qml
)
target_include_directories(
    qindaqt_settings_windows_qml
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_link_libraries(
    qindaqt_settings_windows_qml
    PRIVATE
        qindaqt_settings_windows QindaQt::SettingsClient QindaQt::TokensQml
        Qt6::Core Qt6::DBus Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2
)
set_target_properties(
    qindaqt_settings_windows_qml PROPERTIES
    CXX_EXTENSIONS OFF
    INSTALL_RPATH "$ORIGIN/../../Tokens;$ORIGIN/../../Controls"
)
qindaqt_enable_warnings(qindaqt_settings_windows_qml)

# AGENT-NOTE: The route ships inside the shared SettingsAppearanceRuntime
# component like every other Settings route (see the Accessibility note).
install(
    TARGETS qindaqt_settings_windows_qml qindaqt_settings_windows_qmlplugin
    RUNTIME DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Windows"
        COMPONENT SettingsAppearanceRuntime
    LIBRARY DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Windows"
        COMPONENT SettingsAppearanceRuntime
    ARCHIVE DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Windows"
        COMPONENT SettingsAppearanceRuntime
)
install(
    FILES
        "${QINDAQT_WINDOWS_SETTINGS_QML_OUTPUT_DIRECTORY}/qmldir"
        "${QINDAQT_WINDOWS_SETTINGS_QML_OUTPUT_DIRECTORY}/qindaqt_settings_windows.qmltypes"
        qml/WindowsPage.qml
        qml/WindowsChoiceRow.qml
    DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Windows"
    COMPONENT SettingsAppearanceRuntime
)

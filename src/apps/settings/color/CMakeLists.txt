# SPDX-License-Identifier: GPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS WaylandClient)
find_package(Wayland REQUIRED COMPONENTS Client)

qt_add_library(qindaqt_settings_color STATIC)
target_sources(
    qindaqt_settings_color
    PRIVATE color_application.cpp color_settings_model.cpp color_settings_projection.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/apps/settings_color/color_settings_model.h
)
target_link_libraries(
    qindaqt_settings_color
    PUBLIC QindaQt::DisplayClient QindaQt::DisplayColorAssignment
           QindaQt::DisplayColorDiscovery QindaQt::DisplayWriter Qt6::Core
)
target_compile_features(qindaqt_settings_color PUBLIC cxx_std_20)
set_target_properties(
    qindaqt_settings_color PROPERTIES
    CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON
)
qindaqt_enable_warnings(qindaqt_settings_color)

set(QINDAQT_COLOR_SETTINGS_QML_OUTPUT_DIRECTORY
    "${CMAKE_BINARY_DIR}/qml/QindaQt/SettingsApp/Color")
qt_add_library(qindaqt_settings_color_backend_qml STATIC)
qt_add_qml_module(
    qindaqt_settings_color_backend_qml
    URI QindaQt.SettingsApp.ColorBackend
    VERSION 1.0
    CLASS_NAME QindaQtSettingsColorBackendPlugin
    OUTPUT_DIRECTORY
        "${CMAKE_BINARY_DIR}/qml/QindaQt/SettingsApp/ColorBackend"
    SOURCES
        color_route_composition.h
        color_route_composition.cpp
)
target_link_libraries(
    qindaqt_settings_color_backend_qml
    PRIVATE qindaqt_settings_color QindaQt::TokensQml
            Qt6::Core Qt6::DBus Qt6::Gui Qt6::Qml Qt6::Quick
)
set_target_properties(qindaqt_settings_color_backend_qml PROPERTIES
    CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON)
qindaqt_enable_warnings(qindaqt_settings_color_backend_qml)

qt_add_library(qindaqt_settings_color_qml SHARED)
qt_add_qml_module(
    qindaqt_settings_color_qml
    URI QindaQt.SettingsApp.Color
    VERSION 1.0
    CLASS_NAME QindaQtSettingsColorPlugin
    OUTPUT_DIRECTORY "${QINDAQT_COLOR_SETTINGS_QML_OUTPUT_DIRECTORY}"
    TYPEINFO qindaqt_settings_color.qmltypes
    QML_FILES
        qml/ColorImportSection.qml
        qml/ColorOutputSection.qml
        qml/ColorPage.qml
        qml/ColorProfileSection.qml
)
target_link_libraries(
    qindaqt_settings_color_qml
    PRIVATE QindaQt::TokensQml
            Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick
            Qt6::QuickControls2
)
set_target_properties(
    qindaqt_settings_color_qml PROPERTIES
    CXX_EXTENSIONS OFF
    INSTALL_RPATH "$ORIGIN/../../Tokens;$ORIGIN/../../Controls"
)
qindaqt_enable_warnings(qindaqt_settings_color_qml)

install(
    TARGETS qindaqt_settings_color_qml qindaqt_settings_color_qmlplugin
    RUNTIME DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Color"
        COMPONENT SettingsAppearanceRuntime
    LIBRARY DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Color"
        COMPONENT SettingsAppearanceRuntime
    ARCHIVE DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Color"
        COMPONENT SettingsAppearanceRuntime
)
install(
    FILES
        "${QINDAQT_COLOR_SETTINGS_QML_OUTPUT_DIRECTORY}/qmldir"
        "${QINDAQT_COLOR_SETTINGS_QML_OUTPUT_DIRECTORY}/qindaqt_settings_color.qmltypes"
    DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Color"
    COMPONENT SettingsAppearanceRuntime
)
install(
    FILES
        qml/ColorImportSection.qml
        qml/ColorOutputSection.qml
        qml/ColorPage.qml
        qml/ColorProfileSection.qml
    DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Color"
    COMPONENT SettingsAppearanceRuntime
)

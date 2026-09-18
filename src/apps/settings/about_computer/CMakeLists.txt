# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_library(qindaqt_settings_about_computer STATIC)
target_sources(
    qindaqt_settings_about_computer
    PRIVATE
        about_computer_info.cpp
        about_computer_info_reader.cpp
        about_computer_settings_model.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/apps/settings_about_computer/about_computer_info.h
            include/qindaqt/apps/settings_about_computer/about_computer_info_reader.h
            include/qindaqt/apps/settings_about_computer/about_computer_settings_model.h
)
target_link_libraries(
    qindaqt_settings_about_computer
    PUBLIC Qt6::Core Qt6::DBus Qt6::Gui
)
target_compile_definitions(
    qindaqt_settings_about_computer
    PRIVATE QINDAQT_VERSION="${PROJECT_VERSION}"
)
target_compile_features(qindaqt_settings_about_computer PUBLIC cxx_std_20)
set_target_properties(
    qindaqt_settings_about_computer
    PROPERTIES CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON
)
qindaqt_enable_warnings(qindaqt_settings_about_computer)

set(QINDAQT_ABOUT_COMPUTER_SETTINGS_QML_OUTPUT_DIRECTORY
    "${CMAKE_BINARY_DIR}/qml/QindaQt/SettingsApp/AboutComputer")
qt_add_library(qindaqt_settings_about_computer_qml STATIC)
qt_add_qml_module(
    qindaqt_settings_about_computer_qml
    URI QindaQt.SettingsApp.AboutComputer
    VERSION 1.0
    CLASS_NAME QindaQtSettingsAboutComputerPlugin
    OUTPUT_DIRECTORY "${QINDAQT_ABOUT_COMPUTER_SETTINGS_QML_OUTPUT_DIRECTORY}"
    TYPEINFO qindaqt_settings_about_computer.qmltypes
    SOURCES
        about_computer_route_composition.h
        about_computer_route_composition.cpp
    QML_FILES qml/AboutComputerPage.qml
)
target_link_libraries(
    qindaqt_settings_about_computer_qml
    PRIVATE
        qindaqt_settings_about_computer QindaQt::TokensQml
        Qt6::Core Qt6::DBus Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2
)
set_target_properties(
    qindaqt_settings_about_computer_qml PROPERTIES
    CXX_EXTENSIONS OFF
    INSTALL_RPATH "$ORIGIN/../../Tokens;$ORIGIN/../../Controls"
)
qindaqt_enable_warnings(qindaqt_settings_about_computer_qml)

install(
    TARGETS qindaqt_settings_about_computer_qml qindaqt_settings_about_computer_qmlplugin
    RUNTIME DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/AboutComputer"
        COMPONENT SettingsAppearanceRuntime
    LIBRARY DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/AboutComputer"
        COMPONENT SettingsAppearanceRuntime
    ARCHIVE DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/AboutComputer"
        COMPONENT SettingsAppearanceRuntime
)
install(
    FILES
        "${QINDAQT_ABOUT_COMPUTER_SETTINGS_QML_OUTPUT_DIRECTORY}/qmldir"
        "${QINDAQT_ABOUT_COMPUTER_SETTINGS_QML_OUTPUT_DIRECTORY}/qindaqt_settings_about_computer.qmltypes"
        qml/AboutComputerPage.qml
    DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/AboutComputer"
    COMPONENT SettingsAppearanceRuntime
)

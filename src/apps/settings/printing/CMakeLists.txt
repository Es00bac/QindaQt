# SPDX-License-Identifier: GPL-3.0-or-later
qt_add_library(qindaqt_settings_printing STATIC)
target_sources(qindaqt_settings_printing
    PRIVATE printing_tool_ports.cpp application_printing_tool_catalog.cpp printing_settings_model.cpp
    PUBLIC FILE_SET HEADERS BASE_DIRS include FILES
        include/qindaqt/apps/settings_printing/printing_tool_ports.h
        include/qindaqt/apps/settings_printing/application_printing_tool_catalog.h
        include/qindaqt/apps/settings_printing/printing_settings_model.h)
target_link_libraries(qindaqt_settings_printing PUBLIC Qt6::Core
    PRIVATE QindaQt::ApplicationCatalog)
target_compile_features(qindaqt_settings_printing PUBLIC cxx_std_20)
set_target_properties(qindaqt_settings_printing PROPERTIES
    CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON)
qindaqt_enable_warnings(qindaqt_settings_printing)

set(QINDAQT_PRINTING_SETTINGS_QML_OUTPUT_DIRECTORY
    "${CMAKE_BINARY_DIR}/qml/QindaQt/SettingsApp/Printing")
qt_add_library(qindaqt_settings_printing_qml SHARED)
qt_add_qml_module(qindaqt_settings_printing_qml
    URI QindaQt.SettingsApp.Printing VERSION 1.0
    CLASS_NAME QindaQtSettingsPrintingPlugin
    OUTPUT_DIRECTORY "${QINDAQT_PRINTING_SETTINGS_QML_OUTPUT_DIRECTORY}"
    TYPEINFO qindaqt_settings_printing.qmltypes
    SOURCES printing_route_composition.h printing_route_composition.cpp
    QML_FILES qml/PrintersScannersPage.qml qml/PrintingToolCard.qml)
target_link_libraries(qindaqt_settings_printing_qml PRIVATE
    qindaqt_settings_printing QindaQt::TokensQml
    Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2)
set_target_properties(qindaqt_settings_printing_qml PROPERTIES
    CXX_EXTENSIONS OFF INSTALL_RPATH "$ORIGIN/../../Tokens;$ORIGIN/../../Controls")
qindaqt_enable_warnings(qindaqt_settings_printing_qml)

install(TARGETS qindaqt_settings_printing_qml qindaqt_settings_printing_qmlplugin
    RUNTIME DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Printing" COMPONENT SettingsAppearanceRuntime
    LIBRARY DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Printing" COMPONENT SettingsAppearanceRuntime
    ARCHIVE DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Printing" COMPONENT SettingsAppearanceRuntime)
install(FILES
    "${QINDAQT_PRINTING_SETTINGS_QML_OUTPUT_DIRECTORY}/qmldir"
    "${QINDAQT_PRINTING_SETTINGS_QML_OUTPUT_DIRECTORY}/qindaqt_settings_printing.qmltypes"
    qml/PrintersScannersPage.qml qml/PrintingToolCard.qml
    DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Printing" COMPONENT SettingsAppearanceRuntime)

# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_library(qindaqt_settings_power STATIC)
target_sources(
    qindaqt_settings_power
    PRIVATE power_settings_model.cpp power_settings_projection.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/apps/settings_power/power_settings_model.h
)
target_link_libraries(
    qindaqt_settings_power
    PUBLIC QindaQt::PowerClient QindaQt::BrightnessModel Qt6::Core
)
target_compile_features(qindaqt_settings_power PUBLIC cxx_std_20)
set_target_properties(
    qindaqt_settings_power PROPERTIES
    CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON
)
qindaqt_enable_warnings(qindaqt_settings_power)

set(QINDAQT_POWER_SETTINGS_QML_OUTPUT_DIRECTORY
    "${CMAKE_BINARY_DIR}/qml/QindaQt/SettingsApp/Power")
qt_add_library(qindaqt_settings_power_backend_qml STATIC)
qt_add_qml_module(
    qindaqt_settings_power_backend_qml
    URI QindaQt.SettingsApp.PowerBackend
    VERSION 1.0
    CLASS_NAME QindaQtSettingsPowerBackendPlugin
    OUTPUT_DIRECTORY
        "${CMAKE_BINARY_DIR}/qml/QindaQt/SettingsApp/PowerBackend"
    SOURCES
        power_route_composition.h
        power_route_composition.cpp
)
target_link_libraries(
    qindaqt_settings_power_backend_qml
    PRIVATE qindaqt_settings_power QindaQt::TokensQml
            Qt6::Core Qt6::DBus Qt6::Gui Qt6::Qml Qt6::Quick
)
set_target_properties(qindaqt_settings_power_backend_qml PROPERTIES
    CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON)
qindaqt_enable_warnings(qindaqt_settings_power_backend_qml)

qt_add_library(qindaqt_settings_power_qml SHARED)
qt_add_qml_module(
    qindaqt_settings_power_qml
    URI QindaQt.SettingsApp.Power
    VERSION 1.0
    CLASS_NAME QindaQtSettingsPowerPlugin
    OUTPUT_DIRECTORY "${QINDAQT_POWER_SETTINGS_QML_OUTPUT_DIRECTORY}"
    TYPEINFO qindaqt_settings_power.qmltypes
    QML_FILES
        qml/PowerBrightnessSection.qml
        qml/PowerPage.qml
        qml/PowerProfileSection.qml
        qml/PowerSupplySection.qml
)
target_link_libraries(
    qindaqt_settings_power_qml
    PRIVATE QindaQt::TokensQml
            Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick
            Qt6::QuickControls2
)
set_target_properties(
    qindaqt_settings_power_qml PROPERTIES
    CXX_EXTENSIONS OFF
    INSTALL_RPATH "$ORIGIN/../../Tokens;$ORIGIN/../../Controls"
)
qindaqt_enable_warnings(qindaqt_settings_power_qml)

install(
    TARGETS qindaqt_settings_power_qml qindaqt_settings_power_qmlplugin
    RUNTIME DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Power"
        COMPONENT SettingsAppearanceRuntime
    LIBRARY DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Power"
        COMPONENT SettingsAppearanceRuntime
    ARCHIVE DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Power"
        COMPONENT SettingsAppearanceRuntime
)
install(
    FILES
        "${QINDAQT_POWER_SETTINGS_QML_OUTPUT_DIRECTORY}/qmldir"
        "${QINDAQT_POWER_SETTINGS_QML_OUTPUT_DIRECTORY}/qindaqt_settings_power.qmltypes"
    DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Power"
    COMPONENT SettingsAppearanceRuntime
)
install(
    FILES
        qml/PowerBrightnessSection.qml
        qml/PowerPage.qml
        qml/PowerProfileSection.qml
        qml/PowerSupplySection.qml
    DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Power"
    COMPONENT SettingsAppearanceRuntime
)

# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_library(qindaqt_settings_datetime STATIC)
target_sources(
    qindaqt_settings_datetime
    PRIVATE
        datetime_settings_model.cpp
        systemd_time_service.cpp
        systemd_time_service.h
        week_start_preference.cpp
        week_start_preference.h
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/apps/settings_datetime/datetime_settings_model.h
            include/qindaqt/apps/settings_datetime/system_time_service.h
)
target_link_libraries(
    qindaqt_settings_datetime
    PUBLIC QindaQt::SettingsClient Qt6::Core
    # timedate1 and locale1 are system-bus services; Settings1 is the user's
    # own. QtDBus is private: no consumer of this library sees a bus type.
    PRIVATE Qt6::DBus
)
target_compile_features(qindaqt_settings_datetime PUBLIC cxx_std_20)
set_target_properties(
    qindaqt_settings_datetime
    PROPERTIES CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON
)
qindaqt_enable_warnings(qindaqt_settings_datetime)

set(QINDAQT_DATETIME_SETTINGS_QML_OUTPUT_DIRECTORY
    "${CMAKE_BINARY_DIR}/qml/QindaQt/SettingsApp/DateTime")
qt_add_library(qindaqt_settings_datetime_qml STATIC)
qt_add_qml_module(
    qindaqt_settings_datetime_qml
    URI QindaQt.SettingsApp.DateTime
    VERSION 1.0
    CLASS_NAME QindaQtSettingsDateTimePlugin
    OUTPUT_DIRECTORY "${QINDAQT_DATETIME_SETTINGS_QML_OUTPUT_DIRECTORY}"
    TYPEINFO qindaqt_settings_datetime.qmltypes
    SOURCES
        datetime_route_composition.h
        datetime_route_composition.cpp
    QML_FILES qml/DateTimePage.qml
)
target_link_libraries(
    qindaqt_settings_datetime_qml
    PRIVATE
        qindaqt_settings_datetime QindaQt::SettingsClient QindaQt::TokensQml
        Qt6::Core Qt6::DBus Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2
)
set_target_properties(
    qindaqt_settings_datetime_qml PROPERTIES
    CXX_EXTENSIONS OFF
    INSTALL_RPATH "$ORIGIN/../../Tokens;$ORIGIN/../../Controls"
)
qindaqt_enable_warnings(qindaqt_settings_datetime_qml)

install(
    TARGETS qindaqt_settings_datetime_qml qindaqt_settings_datetime_qmlplugin
    RUNTIME DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/DateTime"
        COMPONENT SettingsAppearanceRuntime
    LIBRARY DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/DateTime"
        COMPONENT SettingsAppearanceRuntime
    ARCHIVE DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/DateTime"
        COMPONENT SettingsAppearanceRuntime
)
install(
    FILES
        "${QINDAQT_DATETIME_SETTINGS_QML_OUTPUT_DIRECTORY}/qmldir"
        "${QINDAQT_DATETIME_SETTINGS_QML_OUTPUT_DIRECTORY}/qindaqt_settings_datetime.qmltypes"
        qml/DateTimePage.qml
    DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/DateTime"
    COMPONENT SettingsAppearanceRuntime
)

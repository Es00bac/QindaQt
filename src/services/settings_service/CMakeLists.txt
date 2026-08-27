# SPDX-License-Identifier: LGPL-3.0-or-later

qt_add_library(qindaqt_settings_service STATIC)
add_library(QindaQt::SettingsService ALIAS qindaqt_settings_service)
target_sources(
    qindaqt_settings_service
    PRIVATE
        src/dbus_service_name_validation.cpp
        src/settings_object.cpp
        src/settings_repository.cpp
        src/resident_settings_service.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/services/settings_service/resident_settings_service.h
            include/qindaqt/services/settings_service/settings_repository.h
)
target_link_libraries(
    qindaqt_settings_service
    PUBLIC QindaQt::Settings QindaQt::SettingsProtocol Qt6::Core Qt6::DBus
)
target_compile_features(qindaqt_settings_service PUBLIC cxx_std_20)
set_target_properties(qindaqt_settings_service PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_settings_service)

qt_add_executable(qindaqt-settings-service src/main.cpp)
target_link_libraries(qindaqt-settings-service PRIVATE QindaQt::SettingsService Qt6::Core Qt6::DBus)
target_compile_definitions(
    qindaqt-settings-service
    PRIVATE
        QINDAQT_SOURCE_SETTINGS_DIR="${PROJECT_SOURCE_DIR}/data/settings"
)
qindaqt_enable_warnings(qindaqt-settings-service)

configure_file(
    data/org.qindaqt.Settings1.service.in
    "${CMAKE_CURRENT_BINARY_DIR}/org.qindaqt.Settings1.service"
    @ONLY
)

install(
    TARGETS qindaqt_settings_service qindaqt-settings-service
    EXPORT QindaQtTargets
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    FILE_SET HEADERS DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/org.qindaqt.Settings1.service"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/dbus-1/services"
)

# SPDX-License-Identifier: LGPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Core DBus)
# AGENT-NOTE: KConfig (KF6::Config::Core) is the only KDE dependency and it is
# the writer for the two owned files: a plain INI writer cannot reproduce the
# KConfig dialect (QSettings escapes the [General] group as [%General],
# which KConfigXT readers would silently miss). Precedent: the session
# powerdevil_idle adapter links KF6Config the same way.
find_package(KF6Config 6.0 REQUIRED CONFIG)

qt_add_library(qindaqt_night_light STATIC)
add_library(QindaQt::NightLight ALIAS qindaqt_night_light)

target_sources(
    qindaqt_night_light
    PRIVATE
        src/night_light_values.cpp
        src/night_light_schedule.cpp
        src/night_light_schedule_client.cpp
        src/qt_config_night_light_port.cpp
        src/qt_night_light_state_port.cpp
        src/night_time_schedule_monitor.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/services/night_light/night_light_values.h
            include/qindaqt/services/night_light/night_light_schedule.h
            include/qindaqt/services/night_light/night_light_schedule_client.h
            include/qindaqt/services/night_light/night_light_config_port.h
            include/qindaqt/services/night_light/night_light_state_port.h
            include/qindaqt/services/night_light/night_time_schedule_monitor.h
)
target_link_libraries(qindaqt_night_light
    PUBLIC Qt6::Core Qt6::DBus
    PRIVATE KF6::ConfigCore QindaQt::CompositorNames
    PUBLIC QindaQt::SettingsClient)
target_compile_features(qindaqt_night_light PUBLIC cxx_std_20)
set_target_properties(qindaqt_night_light
    PROPERTIES CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON)
qindaqt_enable_warnings(qindaqt_night_light)

install(
    TARGETS qindaqt_night_light
    EXPORT QindaQtTargets
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

# The resident scheduler is the only night-light component that composes the
# public Settings1 and Display1 clients. Keep those transports out of the
# values/state library consumed by Settings and other UI modules.
qt_add_library(qindaqt_night_light_schedule_service STATIC)
add_library(QindaQt::NightLightScheduleService ALIAS qindaqt_night_light_schedule_service)
target_sources(qindaqt_night_light_schedule_service
    PRIVATE src/night_light_schedule_service.cpp
    PUBLIC FILE_SET HEADERS
    BASE_DIRS include
    FILES include/qindaqt/services/night_light/night_light_schedule_service.h)
target_link_libraries(qindaqt_night_light_schedule_service
    PUBLIC QindaQt::NightLight
    PRIVATE Qt6::DBus QindaQt::SettingsClient QindaQt::DisplayClient)
target_compile_features(qindaqt_night_light_schedule_service PUBLIC cxx_std_20)
set_target_properties(qindaqt_night_light_schedule_service
    PROPERTIES CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON)
qindaqt_enable_warnings(qindaqt_night_light_schedule_service)
install(TARGETS qindaqt_night_light_schedule_service
    EXPORT QindaQtTargets FILE_SET HEADERS DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

qt_add_executable(qindaqt-night-light-service app/main.cpp)
target_link_libraries(qindaqt-night-light-service
    PRIVATE QindaQt::NightLightScheduleService QindaQt::NightLight
            QindaQt::SettingsClient QindaQt::DisplayClient Qt6::Core Qt6::DBus)
qindaqt_enable_warnings(qindaqt-night-light-service)

configure_file(data/org.qindaqt.NightLight.service.in
    "${CMAKE_CURRENT_BINARY_DIR}/org.qindaqt.NightLight.service" @ONLY)
install(TARGETS qindaqt-night-light-service RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/org.qindaqt.NightLight.service"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/dbus-1/services")
install(FILES data/org.qindaqt.NightLight.xml
    DESTINATION "${CMAKE_INSTALL_DATADIR}/dbus-1/interfaces")

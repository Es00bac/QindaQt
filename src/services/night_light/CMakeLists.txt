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
        src/qt_config_night_light_port.cpp
        src/qt_night_light_state_port.cpp
        src/night_time_schedule_monitor.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/services/night_light/night_light_values.h
            include/qindaqt/services/night_light/night_light_config_port.h
            include/qindaqt/services/night_light/night_light_state_port.h
            include/qindaqt/services/night_light/night_time_schedule_monitor.h
)
target_link_libraries(qindaqt_night_light
    PUBLIC Qt6::Core Qt6::DBus
    PRIVATE KF6::ConfigCore)
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

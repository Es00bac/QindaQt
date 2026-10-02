# SPDX-License-Identifier: GPL-3.0-or-later

find_package(KWayland REQUIRED CONFIG)

qt_add_library(qindaqt_idle_policy STATIC)
add_library(QindaQt::IdlePolicy ALIAS qindaqt_idle_policy)
target_sources(qindaqt_idle_policy
    PRIVATE
        src/display_off_stage.cpp
        src/source_preferences.cpp
        src/shared_preferences.cpp
        src/dim_stage.cpp
        src/idle_suspend_stage.cpp
        production/kwayland_dpms_controller.cpp
        production/kwayland_dpms_controller.h
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/session/idle_policy/display_off_stage.h
            include/qindaqt/session/idle_policy/attached_display_power_port.h
            include/qindaqt/session/idle_policy/source_preferences.h
            include/qindaqt/session/idle_policy/shared_preferences.h
            include/qindaqt/session/idle_policy/dim_stage.h
            include/qindaqt/session/idle_policy/idle_suspend_stage.h)
target_include_directories(qindaqt_idle_policy PRIVATE production)
target_link_libraries(qindaqt_idle_policy
    PUBLIC Qt6::Core QindaQt::IdleObservation QindaQt::PowerClient QindaQt::SettingsClient
    PRIVATE Plasma::KWaylandClient)
target_compile_features(qindaqt_idle_policy PUBLIC cxx_std_20)
set_target_properties(qindaqt_idle_policy PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_idle_policy)
install(TARGETS qindaqt_idle_policy EXPORT QindaQtTargets FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

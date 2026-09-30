# SPDX-License-Identifier: GPL-3.0-or-later

# Shared screen-lock Settings1 model for the Power and Screen saver routes.
# The legacy file adapter remains isolated for migration compatibility; these
# production routes use only confirmed Settings1 lock.* values.

qt_add_library(qindaqt_settings_screen_lock STATIC)
target_sources(
    qindaqt_settings_screen_lock
    PRIVATE
        screen_lock_settings.cpp
        qt_screen_lock_configurator.cpp
        settings1_screen_lock_settings_model.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/apps/settings_screen_lock/screen_lock_settings.h
            include/qindaqt/apps/settings_screen_lock/settings1_screen_lock_settings_model.h
)
target_link_libraries(
    qindaqt_settings_screen_lock
    PUBLIC Qt6::Core QindaQt::LockPreferences QindaQt::SettingsClient QindaQt::SettingsProtocol
    PRIVATE Qt6::DBus
)
target_compile_features(qindaqt_settings_screen_lock PUBLIC cxx_std_20)
set_target_properties(
    qindaqt_settings_screen_lock
    PROPERTIES CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON
)
qindaqt_enable_warnings(qindaqt_settings_screen_lock)

install(
    TARGETS qindaqt_settings_screen_lock
    EXPORT QindaQtTargets
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

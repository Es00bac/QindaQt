# SPDX-License-Identifier: GPL-3.0-or-later

# The KScreenLocker preference boundary (kscreenlockerrc [Daemon] plus the
# live configure request), shared by every Settings route that surfaces idle
# locking: the Power route's Screen lock section (ADR-0091/ADR-0132) and the
# Screen saver route's walk-away section (ADR-0226). One store and model, one
# merge-latest semantics, whichever page the choice is made from.

qt_add_library(qindaqt_settings_screen_lock STATIC)
target_sources(
    qindaqt_settings_screen_lock
    PRIVATE
        screen_lock_settings.cpp
        qt_screen_lock_configurator.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/apps/settings_screen_lock/screen_lock_settings.h
)
target_link_libraries(
    qindaqt_settings_screen_lock
    PUBLIC Qt6::Core
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

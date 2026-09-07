# SPDX-License-Identifier: GPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Core Gui DBus)

qt_add_library(qindaqt_desktop_controls STATIC)
add_library(QindaQt::DesktopControls ALIAS qindaqt_desktop_controls)

target_sources(
    qindaqt_desktop_controls
    PRIVATE
        src/brightness_key_controller.cpp
        src/desktop_shortcut_set.cpp
        src/freedesktop_feedback_notifier.cpp
        src/idle_display_preferences.cpp
        src/idle_display_policy.cpp
        src/powerdevil_idle_preferences_binding.cpp
        src/screenshot_launcher.cpp
        src/settings1_idle_preferences.cpp
        src/volume_key_controller.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/session/desktop_controls/brightness_key_controller.h
            include/qindaqt/session/desktop_controls/desktop_shortcut_set.h
            include/qindaqt/session/desktop_controls/dpms_controller.h
            include/qindaqt/session/desktop_controls/freedesktop_feedback_notifier.h
            include/qindaqt/session/desktop_controls/idle_display_preferences.h
            include/qindaqt/session/desktop_controls/idle_display_policy.h
            include/qindaqt/session/desktop_controls/idle_tracker.h
            include/qindaqt/session/desktop_controls/powerdevil_idle_preferences_binding.h
            include/qindaqt/session/desktop_controls/screenshot_launcher.h
            include/qindaqt/session/desktop_controls/settings1_idle_preferences.h
            include/qindaqt/session/desktop_controls/shortcut_registration.h
            include/qindaqt/session/desktop_controls/volume_key_controller.h
)
target_link_libraries(
    qindaqt_desktop_controls
    PUBLIC
        QindaQt::AudioClient
        QindaQt::PowerService
        QindaQt::PowerDevilIdle
        QindaQt::SettingsClient
        Qt6::Core
        Qt6::Gui
        Qt6::DBus
)
target_compile_features(qindaqt_desktop_controls PUBLIC cxx_std_20)
set_target_properties(qindaqt_desktop_controls PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_desktop_controls)

install(
    TARGETS qindaqt_desktop_controls
    EXPORT QindaQtTargets
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

# The production process needs the compositor-provided KGlobalAccel service and
# the session-owned PowerDevil idle preference adapter. Missing frameworks
# degrade to no resident process while the library and focused tests build.
find_package(KF6GlobalAccel 6.0 CONFIG)

if(TARGET KF6::GlobalAccel AND TARGET QindaQt::PowerDevilIdle)
    qt_add_library(qindaqt_desktop_controls_production STATIC)
    add_library(QindaQt::DesktopControlsProduction ALIAS
        qindaqt_desktop_controls_production)

    target_sources(
        qindaqt_desktop_controls_production
        PRIVATE
            production/kglobal_accel_registrar.cpp
            production/kglobal_accel_registrar.h
    )
    target_include_directories(
        qindaqt_desktop_controls_production PUBLIC production)
    target_link_libraries(
        qindaqt_desktop_controls_production
        PUBLIC
            QindaQt::DesktopControls
            KF6::GlobalAccel
            QindaQt::PowerDevilIdle
            Qt6::Core
            Qt6::Gui
    )
    set_target_properties(qindaqt_desktop_controls_production
        PROPERTIES CXX_EXTENSIONS OFF)
    qindaqt_enable_warnings(qindaqt_desktop_controls_production)

    qt_add_executable(qindaqt-desktop-controls app/main.cpp)
    target_link_libraries(
        qindaqt-desktop-controls
        PRIVATE
            QindaQt::DesktopControls
            QindaQt::DesktopControlsProduction
            QindaQt::AudioClient
            QindaQt::PowerService
            QindaQt::SettingsClient
            Qt6::Core
            Qt6::Gui
            Qt6::DBus
    )
    target_compile_definitions(
        qindaqt-desktop-controls PRIVATE QINDAQT_VERSION="${PROJECT_VERSION}")
    set_target_properties(qindaqt-desktop-controls PROPERTIES CXX_EXTENSIONS OFF)
    qindaqt_enable_warnings(qindaqt-desktop-controls)

    install(TARGETS qindaqt-desktop-controls
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
else()
    message(
        STATUS
            "qindaqt-desktop-controls: KF6GlobalAccel/PowerDevilIdle not all found; resident process not built"
    )
endif()

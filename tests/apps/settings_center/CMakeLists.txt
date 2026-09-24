# SPDX-License-Identifier: GPL-3.0-or-later
find_package(Qt6 REQUIRED COMPONENTS QuickTest Test WaylandClient)
find_package(Wayland REQUIRED COMPONENTS Client)

# 1. Route Registry unit test
qt_add_executable(
    qindaqt_settings_route_registry_test
    tst_settings_route_registry.cpp
)
target_include_directories(qindaqt_settings_route_registry_test PRIVATE ${PROJECT_SOURCE_DIR})
target_link_libraries(
    qindaqt_settings_route_registry_test PRIVATE
    QindaQt::SettingsNavigation Qt6::Core Qt6::Test
)
qindaqt_enable_warnings(qindaqt_settings_route_registry_test)
add_test(
    NAME qindaqt.settings-route-registry
    COMMAND qindaqt_settings_route_registry_test
)
set_tests_properties(
    qindaqt.settings-route-registry PROPERTIES
    LABELS "settings;navigation"
)

# 2. Navigation Controller unit test
qt_add_executable(
    qindaqt_settings_navigation_controller_test
    tst_settings_navigation_controller.cpp
)
target_include_directories(qindaqt_settings_navigation_controller_test PRIVATE ${PROJECT_SOURCE_DIR})
target_link_libraries(
    qindaqt_settings_navigation_controller_test PRIVATE
    QindaQt::SettingsNavigation Qt6::Core Qt6::Test
)
qindaqt_enable_warnings(qindaqt_settings_navigation_controller_test)
add_test(
    NAME qindaqt.settings-navigation-controller
    COMMAND qindaqt_settings_navigation_controller_test
)
set_tests_properties(
    qindaqt.settings-navigation-controller PROPERTIES
    LABELS "settings;navigation"
)

# 3. Offscreen Settings host rows, split by layout and interaction behavior.
qt_add_executable(qindaqt_settings_navigation_layout_test
    tst_settings_navigation_layout.cpp
    settings_navigation_page_test_support.h
    settings_navigation_page_fixture.h
    ${PROJECT_SOURCE_DIR}/tests/apps/settings/audio/stub_audio_settings_model.h
    ${PROJECT_SOURCE_DIR}/tests/apps/settings/network/stub_network_settings_model.h
    ${PROJECT_SOURCE_DIR}/tests/apps/settings/bluetooth/stub_bluetooth_settings_model.h
    ${PROJECT_SOURCE_DIR}/tests/apps/settings/power/stub_power_settings_model.h
    ${PROJECT_SOURCE_DIR}/tests/apps/settings/clipboard/stub_clipboard_settings_model.h
    ${PROJECT_SOURCE_DIR}/tests/apps/settings/customize/stub_customize_settings_model.h
)
target_include_directories(qindaqt_settings_navigation_layout_test PRIVATE ${PROJECT_SOURCE_DIR})
target_link_libraries(qindaqt_settings_navigation_layout_test PRIVATE
    QindaQt::ShellIcons qindaqt_shell_iconsplugin
    QindaQt::SettingsNavigation qindaqt_settings_audio qindaqt_settings_appearance qindaqt_settings_display
    qindaqt_settings_network qindaqt_settings_customize_qml qindaqt_settings_customize_qmlplugin
    qindaqt_settings_bluetooth qindaqt_settings_power qindaqt_settings_power_backend_qml
    qindaqt_settings_power_backend_qmlplugin qindaqt_settings_clipboard_qml
    qindaqt_settings_windows_qml
    qindaqt_settings_clipboard_qmlplugin qindaqt_settings_color_backend_qml
    qindaqt_settings_windows_qmlplugin
    qindaqt_settings_color_backend_qmlplugin qindaqt_settings_input_backend_qml
    qindaqt_settings_input_backend_qmlplugin
    qindaqt_settings_streaming_backend_qml
    qindaqt_settings_streaming_backend_qmlplugin
    qindaqt_settings_datetime_qml qindaqt_settings_datetime_qmlplugin
    qindaqt_settings_default_apps_qml qindaqt_settings_default_apps_qmlplugin
    qindaqt_settings_about_computer_qml qindaqt_settings_about_computer_qmlplugin
    qindaqt_design_tokens qindaqt_tokens_qml
    qindaqt_settings_input_backend_qmlplugin qindaqt_settings_startup_qml
    qindaqt_settings_startup_qmlplugin qindaqt_settings_screensaver_qml
    qindaqt_settings_screensaver_qmlplugin qindaqt_design_tokens qindaqt_tokens_qml
    qindaqt_settings_startup_qmlplugin qindaqt_design_tokens qindaqt_tokens_qml
    qindaqt_settings_login_screen_qml qindaqt_settings_login_screen_qmlplugin
    qindaqt_settings_startup_qmlplugin qindaqt_settings_voice_qml
    qindaqt_settings_voice_qmlplugin qindaqt_design_tokens qindaqt_tokens_qml
    qindaqt_controls_qml qindaqt_themes Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Test
)
target_compile_definitions(qindaqt_settings_navigation_layout_test PRIVATE
    QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
    QINDAQT_BUILD_ROOT="${CMAKE_BINARY_DIR}"
    QINDAQT_BUILD_QML_IMPORT_PATH="${CMAKE_BINARY_DIR}/qml"
    QINDAQT_SETTINGS_SOURCE_DIR="${PROJECT_SOURCE_DIR}/src/apps/settings_center"
    QINDAQT_INSTALL_QML_RELATIVE_PATH="../${QT6_INSTALL_QML}"
    QINDAQT_INSTALL_THEME_RELATIVE_PATH="../${CMAKE_INSTALL_DATADIR}/qindaqt/themes"
)
add_dependencies(qindaqt_settings_navigation_layout_test
    qindaqt-settings qindaqt_settings_appearance_qml qindaqt_settings_appearance_qmlplugin
    qindaqt_settings_display_qml qindaqt_settings_display_qmlplugin qindaqt_settings_network_qml
    qindaqt_settings_network_qmlplugin qindaqt_settings_customize_qml qindaqt_settings_customize_qmlplugin
    qindaqt_settings_audio_qml qindaqt_settings_audio_qmlplugin qindaqt_settings_bluetooth_qml
    qindaqt_settings_bluetooth_qmlplugin qindaqt_settings_power_backend_qml qindaqt_settings_power_backend_qmlplugin
    qindaqt_settings_power_qml qindaqt_settings_power_qmlplugin qindaqt_settings_clipboard_qml
    qindaqt_settings_windows_qml
    qindaqt_settings_clipboard_qmlplugin qindaqt_settings_color_backend_qml qindaqt_settings_color_backend_qmlplugin
    qindaqt_settings_windows_qmlplugin
    qindaqt_settings_input_qml qindaqt_settings_input_qmlplugin
    qindaqt_settings_streaming_qml qindaqt_settings_streaming_qmlplugin
    qindaqt_settings_datetime_qml qindaqt_settings_datetime_qmlplugin
    qindaqt_settings_default_apps_qml qindaqt_settings_default_apps_qmlplugin
    qindaqt_settings_about_computer_qml qindaqt_settings_about_computer_qmlplugin
    qindaqt_settings_startup_qml qindaqt_settings_startup_qmlplugin
    qindaqt_settings_screensaver_qml qindaqt_settings_screensaver_qmlplugin
    qindaqt_settings_login_screen_qml qindaqt_settings_login_screen_qmlplugin
    qindaqt_settings_voice_qml qindaqt_settings_voice_qmlplugin
    qindaqt_controls_qml qindaqt_controls_qmlplugin qindaqt_tokens_qml qindaqt_tokens_qmlplugin
)
qindaqt_enable_warnings(qindaqt_settings_navigation_layout_test)
add_test(NAME qindaqt.settings-navigation-layout COMMAND qindaqt_settings_navigation_layout_test)
set_tests_properties(qindaqt.settings-navigation-layout PROPERTIES
    LABELS "offscreen;settings;navigation"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_FATAL_WARNINGS=1;DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent;DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent"
)

# 3. Offscreen Settings host rows, split by layout and interaction behavior.
qt_add_executable(qindaqt_settings_navigation_interaction_test
    tst_settings_navigation_interaction.cpp
    settings_navigation_page_test_support.h
    settings_navigation_page_fixture.h
    ${PROJECT_SOURCE_DIR}/tests/apps/settings/audio/stub_audio_settings_model.h
    ${PROJECT_SOURCE_DIR}/tests/apps/settings/network/stub_network_settings_model.h
    ${PROJECT_SOURCE_DIR}/tests/apps/settings/bluetooth/stub_bluetooth_settings_model.h
    ${PROJECT_SOURCE_DIR}/tests/apps/settings/power/stub_power_settings_model.h
    ${PROJECT_SOURCE_DIR}/tests/apps/settings/clipboard/stub_clipboard_settings_model.h
    ${PROJECT_SOURCE_DIR}/tests/apps/settings/customize/stub_customize_settings_model.h
)
target_include_directories(qindaqt_settings_navigation_interaction_test PRIVATE ${PROJECT_SOURCE_DIR})
target_link_libraries(qindaqt_settings_navigation_interaction_test PRIVATE
    QindaQt::ShellIcons qindaqt_shell_iconsplugin
    QindaQt::SettingsNavigation qindaqt_settings_audio qindaqt_settings_appearance qindaqt_settings_display
    qindaqt_settings_network qindaqt_settings_customize_qml qindaqt_settings_customize_qmlplugin
    qindaqt_settings_bluetooth qindaqt_settings_power qindaqt_settings_power_backend_qml
    qindaqt_settings_power_backend_qmlplugin qindaqt_settings_clipboard_qml
    qindaqt_settings_windows_qml
    qindaqt_settings_clipboard_qmlplugin qindaqt_settings_color_backend_qml
    qindaqt_settings_windows_qmlplugin
    qindaqt_settings_color_backend_qmlplugin qindaqt_settings_input_backend_qml
    qindaqt_settings_input_backend_qmlplugin
    qindaqt_settings_streaming_backend_qml
    qindaqt_settings_streaming_backend_qmlplugin
    qindaqt_settings_datetime_qml qindaqt_settings_datetime_qmlplugin
    qindaqt_settings_default_apps_qml qindaqt_settings_default_apps_qmlplugin
    qindaqt_settings_about_computer_qml qindaqt_settings_about_computer_qmlplugin
    qindaqt_design_tokens qindaqt_tokens_qml
    qindaqt_settings_input_backend_qmlplugin qindaqt_settings_startup_qml
    qindaqt_settings_startup_qmlplugin qindaqt_settings_screensaver_qml
    qindaqt_settings_screensaver_qmlplugin qindaqt_design_tokens qindaqt_tokens_qml
    qindaqt_settings_startup_qmlplugin qindaqt_design_tokens qindaqt_tokens_qml
    qindaqt_settings_login_screen_qml qindaqt_settings_login_screen_qmlplugin
    qindaqt_settings_startup_qmlplugin qindaqt_settings_voice_qml
    qindaqt_settings_voice_qmlplugin qindaqt_design_tokens qindaqt_tokens_qml
    qindaqt_controls_qml qindaqt_themes Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Test
)
target_compile_definitions(qindaqt_settings_navigation_interaction_test PRIVATE
    QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
    QINDAQT_BUILD_ROOT="${CMAKE_BINARY_DIR}"
    QINDAQT_BUILD_QML_IMPORT_PATH="${CMAKE_BINARY_DIR}/qml"
    QINDAQT_SETTINGS_SOURCE_DIR="${PROJECT_SOURCE_DIR}/src/apps/settings_center"
    QINDAQT_INSTALL_QML_RELATIVE_PATH="../${QT6_INSTALL_QML}"
    QINDAQT_INSTALL_THEME_RELATIVE_PATH="../${CMAKE_INSTALL_DATADIR}/qindaqt/themes"
)
add_dependencies(qindaqt_settings_navigation_interaction_test
    qindaqt-settings qindaqt_settings_appearance_qml qindaqt_settings_appearance_qmlplugin
    qindaqt_settings_display_qml qindaqt_settings_display_qmlplugin qindaqt_settings_network_qml
    qindaqt_settings_network_qmlplugin qindaqt_settings_customize_qml qindaqt_settings_customize_qmlplugin
    qindaqt_settings_audio_qml qindaqt_settings_audio_qmlplugin qindaqt_settings_bluetooth_qml
    qindaqt_settings_bluetooth_qmlplugin qindaqt_settings_power_backend_qml qindaqt_settings_power_backend_qmlplugin
    qindaqt_settings_power_qml qindaqt_settings_power_qmlplugin qindaqt_settings_clipboard_qml
    qindaqt_settings_windows_qml
    qindaqt_settings_clipboard_qmlplugin qindaqt_settings_color_backend_qml qindaqt_settings_color_backend_qmlplugin
    qindaqt_settings_windows_qmlplugin
    qindaqt_settings_input_qml qindaqt_settings_input_qmlplugin
    qindaqt_settings_streaming_qml qindaqt_settings_streaming_qmlplugin
    qindaqt_settings_datetime_qml qindaqt_settings_datetime_qmlplugin
    qindaqt_settings_default_apps_qml qindaqt_settings_default_apps_qmlplugin
    qindaqt_settings_about_computer_qml qindaqt_settings_about_computer_qmlplugin
    qindaqt_settings_startup_qml qindaqt_settings_startup_qmlplugin
    qindaqt_settings_screensaver_qml qindaqt_settings_screensaver_qmlplugin
    qindaqt_settings_login_screen_qml qindaqt_settings_login_screen_qmlplugin
    qindaqt_settings_voice_qml qindaqt_settings_voice_qmlplugin
    qindaqt_controls_qml qindaqt_controls_qmlplugin qindaqt_tokens_qml qindaqt_tokens_qmlplugin
)
qindaqt_enable_warnings(qindaqt_settings_navigation_interaction_test)
add_test(NAME qindaqt.settings-navigation-interaction COMMAND qindaqt_settings_navigation_interaction_test)
set_tests_properties(qindaqt.settings-navigation-interaction PROPERTIES
    LABELS "offscreen;settings;navigation"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_FATAL_WARNINGS=1;DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent;DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent"
)

# 4. CLI unknown route rejection test
add_test(
    NAME qindaqt.settings-app-rejects-unknown-route
    COMMAND "${CMAKE_COMMAND}"
            -DSETTINGS_EXECUTABLE=$<TARGET_FILE:qindaqt-settings>
            -P "${CMAKE_CURRENT_SOURCE_DIR}/check_invalid_route.cmake"
)
set_tests_properties(
    qindaqt.settings-app-rejects-unknown-route PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software"
)

add_test(
    NAME qindaqt.settings-app-rejects-missing-theme
    COMMAND "${CMAKE_COMMAND}"
            -DSETTINGS_EXECUTABLE=$<TARGET_FILE:qindaqt-settings>
            -DSANDBOX_ROOT=${CMAKE_CURRENT_BINARY_DIR}/missing-theme
            -P "${CMAKE_CURRENT_SOURCE_DIR}/check_missing_theme.cmake"
)
set_tests_properties(
    qindaqt.settings-app-rejects-missing-theme PROPERTIES
    LABELS "offscreen;settings;navigation;poison"
)

# 6. Desktop file identity test
add_test(
    NAME qindaqt.settings-app-desktop-identity
    COMMAND "${CMAKE_COMMAND}"
            -DSETTINGS_SOURCE_DIR=${PROJECT_SOURCE_DIR}/src/apps/settings_center
            -DSETTINGS_EXECUTABLE=$<TARGET_FILE:qindaqt-settings>
            -P "${CMAKE_CURRENT_SOURCE_DIR}/check_desktop_identity.cmake"
)

# 7. Route construction offscreen test
add_test(
    NAME qindaqt.settings-app-route-construction
    COMMAND "${CMAKE_COMMAND}"
            -DSETTINGS_EXECUTABLE=$<TARGET_FILE:qindaqt-settings>
            -DTHEME_DIRECTORY=${PROJECT_SOURCE_DIR}/data/themes
            -DSANDBOX_ROOT=${CMAKE_CURRENT_BINARY_DIR}/route-construction
            -P "${CMAKE_CURRENT_SOURCE_DIR}/check_route_construction.cmake"
)
set_tests_properties(
    qindaqt.settings-app-route-construction PROPERTIES
    LABELS "offscreen;settings;navigation;construction" RUN_SERIAL TRUE TIMEOUT 240
)

# A fake route stays resident without an active Loader witness; residence
# alone must not pass the same script used by build and installed stages.
add_test(
    NAME qindaqt.settings-app-route-construction-rejects-missing-witness
    COMMAND "${CMAKE_COMMAND}"
            -DROUTE_CHECK=${CMAKE_CURRENT_SOURCE_DIR}/check_route_construction.cmake
            -DFAKE_EXECUTABLE=${CMAKE_CURRENT_SOURCE_DIR}/fake_route_probe.sh
            -DTHEME_DIRECTORY=${PROJECT_SOURCE_DIR}/data/themes
            -DSANDBOX_ROOT=${CMAKE_CURRENT_BINARY_DIR}/route-construction-regression
            -P "${CMAKE_CURRENT_SOURCE_DIR}/check_route_construction_regression.cmake"
)
set_tests_properties(
    qindaqt.settings-app-route-construction-rejects-missing-witness PROPERTIES
    LABELS "offscreen;settings;navigation;construction;poison" TIMEOUT 10
)

# The ready diagnostic Loader must never count as a page for an available
# registry route. This QML test drives the witness with both Loader identities.
add_test(
    NAME qindaqt.settings-app-route-witness-fallback
    COMMAND $<TARGET_FILE:Qt6::qmltestrunner>
            -input "${CMAKE_CURRENT_SOURCE_DIR}/tst_route_construction_witness.qml"
)
set_tests_properties(
    qindaqt.settings-app-route-witness-fallback PROPERTIES
    LABELS "offscreen;settings;navigation;construction;poison"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QML_DISABLE_DISK_CACHE=1"
    TIMEOUT 10
)

# AGENT-CONTRACT: SettingsAppearanceRuntime installs the LoginScreen helper
# alongside Settings. A clean build of the executable must also make this
# staged-package prerequisite available before the installed route CTest.
add_dependencies(qindaqt-settings qindaqt-sddm-config-helper)

# 8. Installed staged package test
add_test(
    NAME qindaqt.settings-app-installed-routes
    COMMAND "${CMAKE_COMMAND}"
            -DBUILD_DIRECTORY=${CMAKE_BINARY_DIR}
            -DINSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/installed-settings-stage
            -DINSTALL_BINDIR=${CMAKE_INSTALL_BINDIR}
            -DINSTALL_DATADIR=${CMAKE_INSTALL_DATADIR}
            -DINSTALL_QMLDIR=${QT6_INSTALL_QML}
            -DSETTINGS_EXECUTABLE_NAME=qindaqt-settings${CMAKE_EXECUTABLE_SUFFIX}
            -DCONFIGURATION=$<CONFIG>
            -DROUTE_CHECK=${CMAKE_CURRENT_SOURCE_DIR}/check_route_construction.cmake
            -P "${CMAKE_CURRENT_SOURCE_DIR}/check_installed_routes.cmake"
)
set_tests_properties(
    qindaqt.settings-app-installed-routes PROPERTIES
    LABELS "offscreen;settings;navigation;construction;package" RUN_SERIAL TRUE TIMEOUT 300
)

# Scoped Notifications schedule and DND share one serial Settings1 write lane.
qt_add_executable(qindaqt_notification_schedule_outcomes_test
    tst_notification_schedule_outcomes.cpp)
target_include_directories(qindaqt_notification_schedule_outcomes_test PRIVATE ${PROJECT_SOURCE_DIR})
target_link_libraries(qindaqt_notification_schedule_outcomes_test PRIVATE
    qindaqt_settings_notifications QindaQt::SettingsClient Qt6::Core Qt6::Test)
qindaqt_enable_warnings(qindaqt_notification_schedule_outcomes_test)
add_test(NAME qindaqt.settings-notification-schedule-outcomes
         COMMAND qindaqt_notification_schedule_outcomes_test)
set_tests_properties(qindaqt.settings-notification-schedule-outcomes PROPERTIES
    LABELS "settings;notifications;service"
    ENVIRONMENT "DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent;DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent")

# Applied per-app policy writes reconcile only from a same-lineage snapshot at
# or above revisionAfter; below-floor reads retry within a bounded deadline.
qt_add_executable(qindaqt_notification_application_outcomes_test
    tst_notification_application_outcomes.cpp)
target_include_directories(qindaqt_notification_application_outcomes_test PRIVATE ${PROJECT_SOURCE_DIR})
target_link_libraries(qindaqt_notification_application_outcomes_test PRIVATE
    qindaqt_settings_notifications QindaQt::SettingsClient Qt6::Core Qt6::Test)
qindaqt_enable_warnings(qindaqt_notification_application_outcomes_test)
add_test(NAME qindaqt.settings-notification-application-outcomes
         COMMAND qindaqt_notification_application_outcomes_test)
set_tests_properties(qindaqt.settings-notification-application-outcomes PROPERTIES
    LABELS "settings;notifications;service"
    ENVIRONMENT "DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent;DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent")

# Actual Notifications route with production controllers and a private
# Settings1 fake: same-owner refresh and refusal must never appear as a save.
qt_add_executable(qindaqt_notification_page_admission_test
    tst_notification_page_admission.cpp)
target_include_directories(qindaqt_notification_page_admission_test PRIVATE ${PROJECT_SOURCE_DIR})
target_link_libraries(qindaqt_notification_page_admission_test PRIVATE
    qindaqt_settings_notifications QindaQt::SettingsClient
    qindaqt_settings_appearance qindaqt_themes qindaqt_design_tokens
    qindaqt_controls_qml qindaqt_tokens_qml
    Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Test)
target_compile_definitions(qindaqt_notification_page_admission_test PRIVATE
    QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
    QINDAQT_BUILD_QML_IMPORT_PATH="${CMAKE_BINARY_DIR}/qml"
    QINDAQT_SETTINGS_SOURCE_DIR="${PROJECT_SOURCE_DIR}/src/apps/settings_center")
add_dependencies(qindaqt_notification_page_admission_test
    qindaqt_controls_qmlplugin qindaqt_tokens_qmlplugin)
qindaqt_enable_warnings(qindaqt_notification_page_admission_test)
add_test(NAME qindaqt.settings-notification-page-admission
         COMMAND qindaqt_notification_page_admission_test)
set_tests_properties(qindaqt.settings-notification-page-admission PROPERTIES
    LABELS "offscreen;settings;notifications;service"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent;DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent")

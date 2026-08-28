# SPDX-License-Identifier: GPL-3.0-or-later
find_package(Qt6 REQUIRED COMPONENTS QuickTest Test)

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

# 3. Offscreen Navigation Page test
qt_add_executable(
    qindaqt_settings_navigation_page_test
    tst_settings_navigation_page.cpp
)
target_include_directories(qindaqt_settings_navigation_page_test PRIVATE ${PROJECT_SOURCE_DIR})
target_link_libraries(
    qindaqt_settings_navigation_page_test PRIVATE
    QindaQt::SettingsNavigation
    qindaqt_settings_appearance
    qindaqt_design_tokens
    qindaqt_tokens_qml
    qindaqt_controls_qml
    qindaqt_themes
    Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Test
)
target_compile_definitions(
    qindaqt_settings_navigation_page_test PRIVATE
    QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
    QINDAQT_BUILD_ROOT="${CMAKE_BINARY_DIR}"
    QINDAQT_BUILD_QML_IMPORT_PATH="${CMAKE_BINARY_DIR}/qml"
    QINDAQT_SETTINGS_SOURCE_DIR="${PROJECT_SOURCE_DIR}/src/apps/settings_center"
    QINDAQT_INSTALL_QML_RELATIVE_PATH="../${QT6_INSTALL_QML}"
    QINDAQT_INSTALL_THEME_RELATIVE_PATH="../${CMAKE_INSTALL_DATADIR}/qindaqt/themes"
)
add_dependencies(
    qindaqt_settings_navigation_page_test
    qindaqt-settings
    qindaqt_settings_appearance_qml qindaqt_settings_appearance_qmlplugin
    qindaqt_controls_qml qindaqt_controls_qmlplugin
    qindaqt_tokens_qml qindaqt_tokens_qmlplugin
)
qindaqt_enable_warnings(qindaqt_settings_navigation_page_test)
add_test(
    NAME qindaqt.settings-navigation-page
    COMMAND qindaqt_settings_navigation_page_test
)
set_tests_properties(
    qindaqt.settings-navigation-page PROPERTIES
    LABELS "offscreen;settings;navigation"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software"
)

# 4. QML QuickTest runner
add_test(
    NAME qindaqt.settings-app-offscreen
    COMMAND $<TARGET_FILE:Qt6::qmltestrunner>
            -input "${CMAKE_CURRENT_SOURCE_DIR}/tst_settingsapp.qml"
)
set_tests_properties(
    qindaqt.settings-app-offscreen PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software"
)

# 5. CLI unknown route rejection test
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
    LABELS "offscreen;settings;appearance" RUN_SERIAL TRUE TIMEOUT 15
)

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
    LABELS "offscreen;settings;appearance;package" RUN_SERIAL TRUE TIMEOUT 90
)

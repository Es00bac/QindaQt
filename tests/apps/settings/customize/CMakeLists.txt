# SPDX-License-Identifier: GPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS WaylandClient)
find_package(Wayland REQUIRED COMPONENTS Client)

qt_add_executable(
    qindaqt_settings_customize_model_tests
    tst_customize_settings_model.cpp
    customize_test_support.h
)
target_include_directories(
    qindaqt_settings_customize_model_tests PRIVATE "${PROJECT_SOURCE_DIR}"
)
target_link_libraries(
    qindaqt_settings_customize_model_tests
    PRIVATE qindaqt_settings_customize Qt6::Test
)
qindaqt_enable_warnings(qindaqt_settings_customize_model_tests)
add_test(
    NAME qindaqt.settings-customize-model
    COMMAND qindaqt_settings_customize_model_tests
)
set_tests_properties(
    qindaqt.settings-customize-model PROPERTIES
    LABELS "settings;customize;domain"
)

qt_add_executable(
    qindaqt_settings_customize_page_tests
    tst_customize_page.cpp
    stub_customize_settings_model.h
)
target_include_directories(
    qindaqt_settings_customize_page_tests PRIVATE "${PROJECT_SOURCE_DIR}"
)
target_compile_definitions(
    qindaqt_settings_customize_page_tests PRIVATE
    QINDAQT_CUSTOMIZE_PAGE_QML_PATH="${PROJECT_SOURCE_DIR}/src/apps/settings/customize/qml/CustomizePage.qml"
    QINDAQT_QML_IMPORT_PATH="${CMAKE_BINARY_DIR}/qml"
    QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)
target_link_libraries(
    qindaqt_settings_customize_page_tests
    PRIVATE QindaQt::ShellIcons qindaqt_shell_iconsplugin
            qindaqt_settings_customize_qml qindaqt_settings_customize_qmlplugin
            qindaqt_settings_appearance qindaqt_themes QindaQt::TokensQml
            Qt6::Quick Qt6::QuickControls2 Qt6::Test
)
add_dependencies(
    qindaqt_settings_customize_page_tests
    qindaqt_shell_iconsplugin
    qindaqt_tokens_qmlplugin qindaqt_controls_qmlplugin
)
qindaqt_enable_warnings(qindaqt_settings_customize_page_tests)
add_test(
    NAME qindaqt.settings-customize-page
    COMMAND qindaqt_settings_customize_page_tests
)
set_tests_properties(
    qindaqt.settings-customize-page PROPERTIES
    LABELS "offscreen;settings;customize;accessibility"
    ENVIRONMENT
        "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_LINUX_ACCESSIBILITY_ALWAYS_ON=1;QT_FATAL_WARNINGS=1"
)

qt_add_executable(
    qindaqt_settings_customize_pointer_tests
    tst_customize_pointer.cpp
    customize_test_support.h
)
target_include_directories(
    qindaqt_settings_customize_pointer_tests PRIVATE "${PROJECT_SOURCE_DIR}"
)
target_compile_definitions(
    qindaqt_settings_customize_pointer_tests PRIVATE
    QINDAQT_CUSTOMIZE_PAGE_QML_PATH="${PROJECT_SOURCE_DIR}/src/apps/settings/customize/qml/CustomizePage.qml"
    QINDAQT_QML_IMPORT_PATH="${CMAKE_BINARY_DIR}/qml"
    QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)
target_link_libraries(
    qindaqt_settings_customize_pointer_tests
    PRIVATE QindaQt::ShellIcons qindaqt_shell_iconsplugin
            qindaqt_settings_customize qindaqt_settings_customize_qml qindaqt_settings_customize_qmlplugin
            qindaqt_settings_appearance qindaqt_themes QindaQt::TokensQml
            Qt6::Quick Qt6::QuickControls2 Qt6::Test
)
add_dependencies(
    qindaqt_settings_customize_pointer_tests
    qindaqt_shell_iconsplugin
    qindaqt_tokens_qmlplugin qindaqt_controls_qmlplugin
)
qindaqt_enable_warnings(qindaqt_settings_customize_pointer_tests)
add_test(
    NAME qindaqt.settings-customize-pointer
    COMMAND qindaqt_settings_customize_pointer_tests
)
set_tests_properties(
    qindaqt.settings-customize-pointer PROPERTIES
    LABELS "offscreen;settings;customize;accessibility"
    ENVIRONMENT
        "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_LINUX_ACCESSIBILITY_ALWAYS_ON=1;QT_FATAL_WARNINGS=1"
)

qt_add_executable(
    qindaqt_settings_customize_window_lifecycle_tests
    tst_customize_window_lifecycle.cpp
    stub_customize_settings_model.h
)
target_include_directories(
    qindaqt_settings_customize_window_lifecycle_tests
    PRIVATE "${PROJECT_SOURCE_DIR}"
)
target_compile_definitions(
    qindaqt_settings_customize_window_lifecycle_tests PRIVATE
    QINDAQT_SETTINGS_SOURCE_DIR="${PROJECT_SOURCE_DIR}/src/apps/settings_center"
    QINDAQT_QML_IMPORT_PATH="${CMAKE_BINARY_DIR}/qml"
    QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)
target_link_libraries(
    qindaqt_settings_customize_window_lifecycle_tests
    PRIVATE
        QindaQt::ShellIcons qindaqt_shell_iconsplugin
        QindaQt::SettingsNavigation
        qindaqt_settings_appearance
        qindaqt_settings_display
        qindaqt_settings_network
        qindaqt_settings_customize_qml
        qindaqt_settings_customize_qmlplugin
        # AGENT-NOTE: Main.qml imports every route module; the Power, Clipboard,
        # and Color modules are static, so any in-process Main.qml host must link them.
        qindaqt_settings_power
        qindaqt_settings_power_backend_qml
        qindaqt_settings_power_backend_qmlplugin
        qindaqt_settings_clipboard_qml
        qindaqt_settings_clipboard_qmlplugin
        qindaqt_settings_color_backend_qml
        qindaqt_settings_color_backend_qmlplugin
        qindaqt_themes QindaQt::TokensQml
        Qt6::Quick Qt6::QuickControls2 Qt6::Test
)
add_dependencies(
    qindaqt_settings_customize_window_lifecycle_tests
    qindaqt_shell_iconsplugin
    qindaqt_settings_appearance_qmlplugin
    qindaqt_settings_display_qmlplugin
    qindaqt_settings_network_qmlplugin
    qindaqt_settings_customize_qmlplugin
    qindaqt_settings_audio_qmlplugin
    qindaqt_settings_bluetooth_qmlplugin
    qindaqt_settings_power_qmlplugin
    qindaqt_settings_clipboard_qmlplugin
    qindaqt_settings_color_qmlplugin
    qindaqt_tokens_qmlplugin qindaqt_controls_qmlplugin
)
qindaqt_enable_warnings(qindaqt_settings_customize_window_lifecycle_tests)
add_test(
    NAME qindaqt.settings-customize-window-lifecycle
    COMMAND qindaqt_settings_customize_window_lifecycle_tests
)
set_tests_properties(
    qindaqt.settings-customize-window-lifecycle PROPERTIES
    LABELS "offscreen;settings;customize;lifecycle"
    # AGENT-GUARD: This warning-fatal row constructs Main.qml in-process; both
    # D-Bus addresses are pinned to nonexistent sockets so the host-bus-free
    # claim is exercised on any host, matching the navigation-page row.
    ENVIRONMENT
        "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_LINUX_ACCESSIBILITY_ALWAYS_ON=1;QT_FATAL_WARNINGS=1;DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent;DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent"
)

add_test(
    NAME qindaqt.settings-customize-boundary
    COMMAND "${CMAKE_COMMAND}" -DSOURCE_ROOT=${PROJECT_SOURCE_DIR}
            -P "${CMAKE_CURRENT_SOURCE_DIR}/check_boundary.cmake"
)
set_tests_properties(
    qindaqt.settings-customize-boundary PROPERTIES
    LABELS "settings;customize;boundary"
)

add_test(
    NAME qindaqt.settings-customize-boundary-poison
    COMMAND "${CMAKE_COMMAND}"
            -DPOISON_ROOT=${CMAKE_CURRENT_BINARY_DIR}/boundary-poison
            -DCHECK_SCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/check_boundary.cmake
            -P "${CMAKE_CURRENT_SOURCE_DIR}/check_boundary_negative.cmake"
)
set_tests_properties(
    qindaqt.settings-customize-boundary-poison PROPERTIES
    LABELS "settings;customize;boundary;poison"
)

# The route-specific row deliberately reuses the Settings package harness. It
# gives the Customize selector an installed-prefix proof while keeping route
# construction authority in one hostile, sanitized script.
add_test(
    NAME qindaqt.settings-customize-installed-route
    COMMAND "${CMAKE_COMMAND}"
            -DBUILD_DIRECTORY=${CMAKE_BINARY_DIR}
            -DINSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/installed-customize-stage
            -DINSTALL_BINDIR=${CMAKE_INSTALL_BINDIR}
            -DINSTALL_DATADIR=${CMAKE_INSTALL_DATADIR}
            -DINSTALL_QMLDIR=${QT6_INSTALL_QML}
            -DSETTINGS_EXECUTABLE_NAME=qindaqt-settings${CMAKE_EXECUTABLE_SUFFIX}
            -DCONFIGURATION=$<CONFIG>
            -DROUTE_CHECK=${PROJECT_SOURCE_DIR}/tests/apps/settings_center/check_route_construction.cmake
            -P ${PROJECT_SOURCE_DIR}/tests/apps/settings_center/check_installed_routes.cmake
)
set_tests_properties(
    qindaqt.settings-customize-installed-route PROPERTIES
    LABELS "offscreen;settings;customize;package" RUN_SERIAL TRUE TIMEOUT 120
)

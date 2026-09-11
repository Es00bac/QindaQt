# SPDX-License-Identifier: GPL-3.0-or-later

# AGENT-GUARD: Use Qt's imported runner target. A bare `qmltestrunner` resolves
# to Qt 5 on mixed-Qt distributions and cannot load this Qt 6 module.
find_package(Qt6 REQUIRED COMPONENTS QuickTest Svg)
find_program(DBUS_DAEMON_EXECUTABLE dbus-daemon REQUIRED)

add_test(
    NAME qindaqt.shell-icon-coverage
    COMMAND ${CMAKE_COMMAND}
        -DQINDAQT_SOURCE_DIR=${PROJECT_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/verify_shell_icon_coverage.cmake
)

qt_add_executable(
    qindaqt_shell_icon_configuration_tests
    tst_shelliconconfiguration.cpp
    "${CMAKE_CURRENT_LIST_DIR}/../../src/shell/common/shelliconconfiguration.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../../src/shell/common/shelliconconfiguration.h"
)
target_include_directories(
    qindaqt_shell_icon_configuration_tests
    PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../../src/shell/common"
)
target_link_libraries(
    qindaqt_shell_icon_configuration_tests
    PRIVATE qindaqt_themes Qt6::Test
)
target_compile_definitions(
    qindaqt_shell_icon_configuration_tests
    PRIVATE
        QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
        QINDAQT_TEST_SCRATCH="${CMAKE_CURRENT_BINARY_DIR}/icon-configuration-scratch"
)
qindaqt_enable_warnings(qindaqt_shell_icon_configuration_tests)
add_test(
    NAME qindaqt.shell-icon-runtime-configuration
    COMMAND qindaqt_shell_icon_configuration_tests
)

qt_add_executable(
    qindaqt_notification_quieting_bridge_tests
    tst_notificationquietingsettingsbridge.cpp
    "${CMAKE_CURRENT_LIST_DIR}/../../src/shell/runtime/notificationquietingsettingsbridge.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../../src/shell/runtime/notificationquietingsettingsbridge.h"
    "${CMAKE_CURRENT_LIST_DIR}/../../src/shell/runtime/settingsroutelauncher.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../../src/shell/runtime/settingsroutelauncher.h"
)
target_include_directories(
    qindaqt_notification_quieting_bridge_tests
    PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../../src/shell/runtime"
)
target_link_libraries(
    qindaqt_notification_quieting_bridge_tests
    PRIVATE
        QindaQt::SettingsClient
        QindaQt::SettingsService
        QindaQt::NotificationPresentationPolicy
        Qt6::DBus
        Qt6::Test
)
target_compile_definitions(
    qindaqt_notification_quieting_bridge_tests
    PRIVATE
        QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
        QINDAQT_DBUS_DAEMON_EXECUTABLE="${DBUS_DAEMON_EXECUTABLE}"
)
qindaqt_enable_warnings(qindaqt_notification_quieting_bridge_tests)
add_test(
    NAME qindaqt.notification-quieting-settings-bridge
    COMMAND qindaqt_notification_quieting_bridge_tests
)

qt_add_executable(
    qindaqt_panel_blur_tests
    tst_panel_blur.cpp
    "${CMAKE_CURRENT_LIST_DIR}/../../src/panel_blur/src/panel_surface_blur.cpp"
)
target_include_directories(
    qindaqt_panel_blur_tests
    PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../../src/panel_blur/include"
            "${CMAKE_BINARY_DIR}/src/panel_blur"
)
target_link_libraries(
    qindaqt_panel_blur_tests
    PRIVATE
        QindaQt::PanelBlur
        Qt6::Gui
        Qt6::Quick
        Qt6::Test
)
qindaqt_enable_warnings(qindaqt_panel_blur_tests)
add_test(NAME qindaqt.panel-blur COMMAND qindaqt_panel_blur_tests)

qt_add_executable(qindaqt_shell_capture_tests tst_shell_capture.cpp)

target_link_libraries(
    qindaqt_shell_capture_tests
    PRIVATE
        Qt6::Gui
        Qt6::Test
)

target_compile_definitions(
    qindaqt_shell_capture_tests
    PRIVATE QINDAQT_SHELL_PREVIEW_EXECUTABLE="$<TARGET_FILE:qindaqt-shell-preview>"
)

add_dependencies(qindaqt_shell_capture_tests qindaqt-shell-preview)
qindaqt_enable_warnings(qindaqt_shell_capture_tests)
add_test(NAME qindaqt.shell-capture-matrix COMMAND qindaqt_shell_capture_tests)

add_test(
    NAME qindaqt.decoration-hover
    COMMAND
        $<TARGET_FILE:Qt6::qmltestrunner>
        -input "${CMAKE_CURRENT_SOURCE_DIR}/qml/tst_decorationbuttons.qml"
)

add_test(
    NAME qindaqt.notification-surfaces-offscreen
    COMMAND
        $<TARGET_FILE:Qt6::qmltestrunner>
        -input "${CMAKE_CURRENT_SOURCE_DIR}/qml/tst_notificationsurfaces.qml"
)

add_test(
    NAME qindaqt.notification-focus-offscreen
    COMMAND
        $<TARGET_FILE:Qt6::qmltestrunner>
        -input "${CMAKE_CURRENT_SOURCE_DIR}/qml/tst_notificationfocus.qml"
)

add_test(
    NAME qindaqt.notification-quieting-controls-offscreen
    COMMAND
        $<TARGET_FILE:Qt6::qmltestrunner>
        -input "${CMAKE_CURRENT_SOURCE_DIR}/qml/tst_notificationquietingcontrols.qml"
)

add_test(
    NAME qindaqt.notification-center-applet-offscreen
    COMMAND
        $<TARGET_FILE:Qt6::qmltestrunner>
        -import "${CMAKE_CURRENT_SOURCE_DIR}/qml/imports"
        -input "${CMAKE_CURRENT_SOURCE_DIR}/qml/tst_notificationcenterapplet.qml"
)

add_test(
    NAME qindaqt.panel-geometry-offscreen
    COMMAND $<TARGET_FILE:Qt6::qmltestrunner>
        -import "${CMAKE_CURRENT_SOURCE_DIR}/qml/imports"
        -input "${CMAKE_CURRENT_SOURCE_DIR}/qml/tst_panelgeometry.qml"
)

add_test(
    NAME qindaqt.luna-taskbar-offscreen
    COMMAND $<TARGET_FILE:Qt6::qmltestrunner>
        -import "${CMAKE_CURRENT_SOURCE_DIR}/qml/imports"
        -input "${CMAKE_CURRENT_SOURCE_DIR}/qml/tst_lunataskbar.qml"
)

set_tests_properties(
    qindaqt.panel-geometry-offscreen
    qindaqt.luna-taskbar-offscreen
    qindaqt.decoration-hover
    qindaqt.notification-surfaces-offscreen
    qindaqt.notification-focus-offscreen
    qindaqt.notification-quieting-controls-offscreen
    qindaqt.notification-center-applet-offscreen
    PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software"
)

if(TARGET qindaqt-shell)
    qt_add_executable(
        qindaqt_catalog_paths_tests
        tst_catalogpaths.cpp
        "${CMAKE_CURRENT_LIST_DIR}/../../src/shell/common/catalogpaths.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../../src/shell/common/catalogpaths.h"
    )
    target_include_directories(
        qindaqt_catalog_paths_tests
        PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../../src/shell/common"
    )
    target_link_libraries(
        qindaqt_catalog_paths_tests
        PRIVATE Qt6::Core Qt6::Test
    )
    qindaqt_enable_warnings(qindaqt_catalog_paths_tests)
    add_test(NAME qindaqt.shell-catalog-paths COMMAND qindaqt_catalog_paths_tests)

    qt_add_executable(
        qindaqt_shell_preference_values_tests
        tst_shellpreferencevalues.cpp
        "${CMAKE_CURRENT_LIST_DIR}/../../src/shell/runtime/shellpreferencevalues.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../../src/shell/runtime/shellpreferencevalues.h"
    )
    target_include_directories(
        qindaqt_shell_preference_values_tests
        PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../../src/shell/runtime"
    )
    target_link_libraries(
        qindaqt_shell_preference_values_tests
        PRIVATE QindaQt::DesignTokens Qt6::Core Qt6::Test
    )
    qindaqt_enable_warnings(qindaqt_shell_preference_values_tests)
    add_test(
        NAME qindaqt.shell-preference-values
        COMMAND qindaqt_shell_preference_values_tests
    )

    qt_add_executable(
        qindaqt_shell_startup_preferences_tests
        tst_shellstartuppreferences.cpp
        "${CMAKE_CURRENT_LIST_DIR}/../../src/shell/runtime/shellstartuppreferences.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../../src/shell/runtime/shellstartuppreferences.h"
        "${CMAKE_CURRENT_LIST_DIR}/../../src/shell/runtime/shellpreferencevalues.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../../src/shell/runtime/shellpreferencevalues.h"
    )
    target_include_directories(
        qindaqt_shell_startup_preferences_tests
        PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../../src/shell/runtime"
    )
    target_link_libraries(
        qindaqt_shell_startup_preferences_tests
        PRIVATE
            QindaQt::DesignTokens
            QindaQt::SettingsClient
            QindaQt::SettingsService
            Qt6::DBus
            Qt6::Test
    )
    target_compile_definitions(
        qindaqt_shell_startup_preferences_tests
        PRIVATE
            QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
            QINDAQT_DBUS_DAEMON_EXECUTABLE="${DBUS_DAEMON_EXECUTABLE}"
    )
    qindaqt_enable_warnings(qindaqt_shell_startup_preferences_tests)
    add_test(
        NAME qindaqt.shell-startup-preferences
        COMMAND qindaqt_shell_startup_preferences_tests
    )

    qt_add_executable(
        qindaqt_runtime_panel_applet_compatibility_tests
        tst_runtimepanelappletcompatibility.cpp
        "${CMAKE_CURRENT_LIST_DIR}/../../src/shell/runtime/runtimepanelappletcompatibility.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../../src/shell/runtime/runtimepanelappletcompatibility.h"
    )
    target_include_directories(
        qindaqt_runtime_panel_applet_compatibility_tests
        PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../../src/shell/runtime"
    )
    target_link_libraries(
        qindaqt_runtime_panel_applet_compatibility_tests
        PRIVATE qindaqt_profiles Qt6::Test
    )
    qindaqt_enable_warnings(qindaqt_runtime_panel_applet_compatibility_tests)
    add_test(
        NAME qindaqt.shell-runtime-panel-applet-compatibility
        COMMAND qindaqt_runtime_panel_applet_compatibility_tests
    )

    qt_add_executable(
        qindaqt_shell_runtime_token_tests
        tst_shellruntime_tokens.cpp
        "${CMAKE_CURRENT_LIST_DIR}/../../src/shell/common/shelltokenpublisher.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../../src/shell/common/shelltokenpublisher.h"
        "${CMAKE_CURRENT_LIST_DIR}/../../src/shell/runtime/shellappearancebridge.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../../src/shell/runtime/shellappearancebridge.h"
        "${CMAKE_CURRENT_LIST_DIR}/../../src/shell/runtime/shellpreferencevalues.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../../src/shell/runtime/shellpreferencevalues.h"
    )
    target_include_directories(
        qindaqt_shell_runtime_token_tests
        PRIVATE
            "${CMAKE_CURRENT_LIST_DIR}/../../src/shell/common"
            "${CMAKE_CURRENT_LIST_DIR}/../../src/shell/runtime"
    )
    target_link_libraries(
        qindaqt_shell_runtime_token_tests
        PRIVATE
            qindaqt_themes
            QindaQt::AppAppearance
            QindaQt::SettingsClient
            QindaQt::TokensQml
            QindaQt::ShellTaskListApplet
            qindaqt_shell_task_list_appletplugin
            Qt6::Gui
            Qt6::Qml
            Qt6::Quick
            Qt6::Test
    )
    target_compile_definitions(
        qindaqt_shell_runtime_token_tests
        PRIVATE
            QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
            QINDAQT_QML_BUILD_DIR="${CMAKE_BINARY_DIR}/qml"
    )
    qindaqt_enable_warnings(qindaqt_shell_runtime_token_tests)
    qt_import_qml_plugins(qindaqt_shell_runtime_token_tests)
    add_dependencies(
        qindaqt_shell_runtime_token_tests
        qindaqt_tokens_qmlplugin
        qindaqt_controls_qmlplugin
    )
    add_test(
        NAME qindaqt.shell-runtime-token-publication
        COMMAND qindaqt_shell_runtime_token_tests
    )
    set_tests_properties(
        qindaqt.shell-runtime-token-publication
        PROPERTIES ENVIRONMENT
            "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_FATAL_WARNINGS=1"
    )

    qt_add_executable(qindaqt_theme_map_propagation_tests tst_thememappropagation.cpp)
    target_include_directories(
        qindaqt_theme_map_propagation_tests
        PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../../src/shell/runtime"
    )
    target_link_libraries(
        qindaqt_theme_map_propagation_tests
        PRIVATE Qt6::Gui Qt6::Quick Qt6::Test
    )
    qindaqt_enable_warnings(qindaqt_theme_map_propagation_tests)
    add_test(
        NAME qindaqt.shell-theme-map-propagation
        COMMAND qindaqt_theme_map_propagation_tests
    )
    set_tests_properties(
        qindaqt.shell-theme-map-propagation
        PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software"
    )

    qt_add_executable(
        qindaqt_shell_runtime_startup_launch_tests
        tst_shellruntime_startuplaunch.cpp
        "${CMAKE_CURRENT_LIST_DIR}/../../src/shell/runtime/shellpreferencevalues.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../../src/shell/runtime/shellpreferencevalues.h"
    )
    target_include_directories(
        qindaqt_shell_runtime_startup_launch_tests
        PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../../src/shell/runtime"
    )
    target_link_libraries(
        qindaqt_shell_runtime_startup_launch_tests
        PRIVATE
            QindaQt::DesignTokens
            QindaQt::SettingsClient
            QindaQt::SettingsService
            Qt6::DBus
            Qt6::Test
    )
    target_compile_definitions(
        qindaqt_shell_runtime_startup_launch_tests
        PRIVATE
            QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
            QINDAQT_DBUS_DAEMON_EXECUTABLE="${DBUS_DAEMON_EXECUTABLE}"
            QINDAQT_SHELL_EXECUTABLE="$<TARGET_FILE:qindaqt-shell>"
    )
    add_dependencies(qindaqt_shell_runtime_startup_launch_tests qindaqt-shell)
    qindaqt_enable_warnings(qindaqt_shell_runtime_startup_launch_tests)
    add_test(
        NAME qindaqt.shell-runtime-startup-launch
        COMMAND qindaqt_shell_runtime_startup_launch_tests
    )

    qt_add_executable(
        qindaqt_notification_output_selector_tests
        tst_notificationoutputselector.cpp
        notification_output_test_support.h
        "${CMAKE_CURRENT_LIST_DIR}/../../src/shell/runtime/compositoroutputauthority.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../../src/shell/runtime/compositoroutputauthority.h"
        "${CMAKE_CURRENT_LIST_DIR}/../../src/shell/runtime/notificationoutputselector.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../../src/shell/runtime/notificationoutputselector.h"
    )
    target_include_directories(
        qindaqt_notification_output_selector_tests
        PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../../src/shell/runtime"
    )
    target_link_libraries(
        qindaqt_notification_output_selector_tests
        PRIVATE
            QindaQt::ShellLayout
            QindaQt::ShellVisibility
            Qt6::Test
    )
    qindaqt_enable_warnings(qindaqt_notification_output_selector_tests)
    add_test(
        NAME qindaqt.notification-output-selector
        COMMAND qindaqt_notification_output_selector_tests
    )

    qt_add_executable(
        qindaqt_qt_compositor_output_authority_tests
        tst_qtcompositoroutputauthority.cpp
        notification_output_test_support.h
        "${CMAKE_CURRENT_LIST_DIR}/../../src/shell/runtime/compositoroutputauthority.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../../src/shell/runtime/compositoroutputauthority.h"
        "${CMAKE_CURRENT_LIST_DIR}/../../src/shell/runtime/qtcompositoroutputauthority.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../../src/shell/runtime/qtcompositoroutputauthority.h"
    )
    target_include_directories(
        qindaqt_qt_compositor_output_authority_tests
        PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../../src/shell/runtime"
    )
    target_link_libraries(
        qindaqt_qt_compositor_output_authority_tests
        PRIVATE
            QindaQt::ShellVisibility
            Qt6::DBus
            Qt6::Test
    )
    qindaqt_enable_warnings(qindaqt_qt_compositor_output_authority_tests)
    add_test(
        NAME qindaqt.notification-output-authority-qt
        COMMAND qindaqt_qt_compositor_output_authority_tests
    )

    qt_add_executable(
        qindaqt_notification_center_entry_tests
        tst_notificationcenterentry.cpp
        "${CMAKE_CURRENT_LIST_DIR}/../../src/shell/runtime/notificationcenterappletaccess.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../../src/shell/runtime/notificationcenterappletaccess.h"
        "${CMAKE_CURRENT_LIST_DIR}/../../src/shell/runtime/notificationcentershortcut.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../../src/shell/runtime/notificationcentershortcut.h"
    )
    target_include_directories(
        qindaqt_notification_center_entry_tests
        PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../../src/shell/runtime"
    )
    target_link_libraries(
        qindaqt_notification_center_entry_tests
        PRIVATE Qt6::Gui Qt6::Test
    )
    qindaqt_enable_warnings(qindaqt_notification_center_entry_tests)
    add_test(
        NAME qindaqt.notification-center-entry
        COMMAND qindaqt_notification_center_entry_tests
    )
    set_tests_properties(
        qindaqt.notification-center-entry
        PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )

    find_package(KF6GlobalAccel 6.0 REQUIRED CONFIG)

    qt_add_executable(
        qindaqt_shortcut_note_tests
        tst_shortcutnote.cpp
        "${CMAKE_CURRENT_LIST_DIR}/../../src/shell/runtime/kglobalaccelshortcutregistrar.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../../src/shell/runtime/kglobalaccelshortcutregistrar.h"
        "${CMAKE_CURRENT_LIST_DIR}/../../src/shell/runtime/shortcutnotecontroller.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../../src/shell/runtime/shortcutnotecontroller.h"
        "${CMAKE_CURRENT_LIST_DIR}/../../src/shell/runtime/shortcutnoteshortcut.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../../src/shell/runtime/shortcutnoteshortcut.h"
    )
    target_include_directories(
        qindaqt_shortcut_note_tests
        PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../../src/shell/runtime"
    )
    target_link_libraries(
        qindaqt_shortcut_note_tests
        PRIVATE
            QindaQt::SettingsClient
            QindaQt::SettingsService
            KF6::GlobalAccel
            Qt6::Core
            Qt6::DBus
            Qt6::Gui
            Qt6::Qml
            Qt6::Quick
            Qt6::Test
    )
    target_compile_definitions(
        qindaqt_shortcut_note_tests
        PRIVATE
            QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
            QINDAQT_DBUS_DAEMON_EXECUTABLE="${DBUS_DAEMON_EXECUTABLE}"
    )
    qindaqt_enable_warnings(qindaqt_shortcut_note_tests)
    add_test(
        NAME qindaqt.shortcut-note
        COMMAND qindaqt_shortcut_note_tests
    )
    set_tests_properties(
        qindaqt.shortcut-note
        PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )

    add_test(
        NAME qindaqt.shortcut-note-card-offscreen
        COMMAND $<TARGET_FILE:Qt6::qmltestrunner>
        -input "${CMAKE_CURRENT_SOURCE_DIR}/qml/tst_shortcutnotecard.qml"
    )
    set_tests_properties(
        qindaqt.shortcut-note-card-offscreen
        PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software"
    )

    add_test(
        NAME qindaqt.shell-runtime-catalog
        COMMAND $<TARGET_FILE:qindaqt-shell> --list
    )
    set_tests_properties(
        qindaqt.shell-runtime-catalog
        PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software"
            PASS_REGULAR_EXPRESSION "notification-center - Notification Center"
    )
endif()

qt_add_executable(
    qindaqt_shell_runtime_layout_adoption_tests
    tst_runtimelayoutadoption.cpp
    "${CMAKE_CURRENT_LIST_DIR}/../../src/shell/runtime/runtime_layout_adoption.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../../src/shell/runtime/runtime_layout_adoption.h"
)
target_include_directories(
    qindaqt_shell_runtime_layout_adoption_tests
    PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../../src/shell/runtime"
)
target_link_libraries(
    qindaqt_shell_runtime_layout_adoption_tests
    PRIVATE qindaqt_profiles Qt6::Test
)
target_compile_definitions(
    qindaqt_shell_runtime_layout_adoption_tests
    PRIVATE QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)
qindaqt_enable_warnings(qindaqt_shell_runtime_layout_adoption_tests)
add_test(
    NAME qindaqt.shell-runtime-layout-adoption
    COMMAND qindaqt_shell_runtime_layout_adoption_tests
)
set_tests_properties(
    qindaqt.shell-runtime-layout-adoption PROPERTIES
    LABELS "shell;runtime;customize"
)

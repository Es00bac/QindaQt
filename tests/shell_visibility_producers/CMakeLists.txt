# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_executable(
    qindaqt_panel_visibility_producer_tests
    tst_panelvisibilityproducers.cpp
)
target_link_libraries(
    qindaqt_panel_visibility_producer_tests
    PRIVATE
        qindaqt_shell_panel_visibility_producers
        QindaQt::ShellOrchestration
        Qt6::Test
)
qindaqt_enable_warnings(qindaqt_panel_visibility_producer_tests)

qt_add_executable(
    qindaqt_panel_visibility_popup_bounds_tests
    tst_panelvisibilitypopupbounds.cpp
)
target_link_libraries(
    qindaqt_panel_visibility_popup_bounds_tests
    PRIVATE
        qindaqt_shell_panel_visibility_producers
        QindaQt::ShellOrchestration
        Qt6::Test
)
qindaqt_enable_warnings(qindaqt_panel_visibility_popup_bounds_tests)

qt_add_executable(
    qindaqt_panel_visibility_settings_private_bus_tests
    tst_panelvisibilitysettingsprivatebus.cpp
)
target_link_libraries(
    qindaqt_panel_visibility_settings_private_bus_tests
    PRIVATE
        qindaqt_shell_panel_visibility_producers
        QindaQt::SettingsClient
        QindaQt::ShellOrchestration
        Qt6::DBus
        Qt6::Test
)
qindaqt_enable_warnings(qindaqt_panel_visibility_settings_private_bus_tests)

foreach(producer IN ITEMS pointer popup shortcut animation)
    add_test(
        NAME qindaqt.shell-visibility-producer-${producer}
        COMMAND qindaqt_panel_visibility_producer_tests ${producer}
    )
    set_tests_properties(
        qindaqt.shell-visibility-producer-${producer}
        PROPERTIES LABELS "shell;visibility;producer"
    )
endforeach()
add_test(
    NAME qindaqt.shell-visibility-producer-reduced-motion
    COMMAND qindaqt_panel_visibility_producer_tests reducedMotion
)
add_test(
    NAME qindaqt.shell-visibility-producer-boundary-poison
    COMMAND qindaqt_panel_visibility_producer_tests boundaryPoison
)
add_test(
    NAME qindaqt.shell-visibility-popup-bounds
    COMMAND qindaqt_panel_visibility_popup_bounds_tests
)
find_program(QINDAQT_PANEL_VISIBILITY_DBUS_RUN_SESSION dbus-run-session REQUIRED)
add_test(
    NAME qindaqt.shell-visibility-settings-private-bus
    COMMAND
        "${QINDAQT_PANEL_VISIBILITY_DBUS_RUN_SESSION}" --
        "$<TARGET_FILE:qindaqt_panel_visibility_settings_private_bus_tests>"
)
set_tests_properties(
    qindaqt.shell-visibility-producer-reduced-motion
    qindaqt.shell-visibility-producer-boundary-poison
    qindaqt.shell-visibility-popup-bounds
    qindaqt.shell-visibility-settings-private-bus
    PROPERTIES LABELS "shell;visibility;producer"
)
set_tests_properties(
    qindaqt.shell-visibility-producer-pointer
    qindaqt.shell-visibility-producer-popup
    qindaqt.shell-visibility-producer-shortcut
    qindaqt.shell-visibility-producer-animation
    qindaqt.shell-visibility-producer-reduced-motion
    qindaqt.shell-visibility-producer-boundary-poison
    qindaqt.shell-visibility-popup-bounds
    qindaqt.shell-visibility-settings-private-bus
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

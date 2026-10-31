# SPDX-License-Identifier: GPL-3.0-or-later

# Firewall panel applet (docs/wiki/shell/firewall-applet.md, ADR-0431): pure
# projection, the controller over the public FirewallClient with a fake
# transport, the compiled QML module offscreen with keyboard-only
# interaction, and the source boundary probe.

find_package(Qt6 6.11 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Svg Test)

function(qindaqt_add_firewall_applet_test target source test_name labels)
    # AGENT-NOTE: the fake transport is a Q_OBJECT header; listing it here is
    # what makes AUTOMOC generate its vtable.
    qt_add_executable(${target} ${source} firewall_applet_test_support.h)
    target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(${target} PRIVATE ${ARGN} Qt6::Test)
    qindaqt_enable_warnings(${target})
    add_test(NAME ${test_name} COMMAND ${target})
    set_tests_properties(${test_name} PROPERTIES LABELS "${labels}")
endfunction()

qindaqt_add_firewall_applet_test(
    qindaqt_firewall_applet_presentation_tests tst_firewall_applet_presentation.cpp
    qindaqt.firewall-applet-presentation "shell;firewall;pure"
    QindaQt::ShellFirewallApplet QindaQt::FirewallClient)
qindaqt_add_firewall_applet_test(
    qindaqt_firewall_applet_controller_tests tst_firewall_applet_controller.cpp
    qindaqt.firewall-applet-controller "shell;firewall;runtime"
    QindaQt::ShellFirewallAppletRuntime)

qindaqt_add_firewall_applet_test(
    qindaqt_firewall_applet_qml_tests tst_firewall_applet_qml.cpp
    qindaqt.firewall-applet-offscreen "shell;firewall;runtime;offscreen;accessibility"
    QindaQt::ShellFirewallAppletRuntime
    qindaqt_shell_firewall_applet_runtimeplugin
    QindaQt::ShellIcons
    qindaqt_shell_iconsplugin
    QindaQt::TokensQml
    qindaqt_themes
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick)
target_compile_definitions(
    qindaqt_firewall_applet_qml_tests
    PRIVATE
        QINDAQT_FIREWALL_APPLET_QML_IMPORT_PATH="${CMAKE_BINARY_DIR}/qml"
        QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
        QINDAQT_DESKTOP_CONTROLS_QML_IMPORT_PATH="${CMAKE_BINARY_DIR}/qml"
        QINDAQT_FIREWALL_APPLET_ICON_FIXTURE_ROOT="${CMAKE_CURRENT_BINARY_DIR}/icon-resolution"
)
target_include_directories(qindaqt_firewall_applet_qml_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/tests/shell/desktop_controls")
add_dependencies(qindaqt_firewall_applet_qml_tests qindaqt_tokens_qmlplugin
                 qindaqt_controls_qmlplugin)
qt_import_qml_plugins(qindaqt_firewall_applet_qml_tests)
# AGENT-NOTE: QML warnings are failures here, as for every Settings QML row.
set_tests_properties(
    qindaqt.firewall-applet-offscreen
    PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_FATAL_WARNINGS=1"
)

add_test(
    NAME qindaqt.firewall-applet-boundary
    COMMAND
        "${CMAKE_COMMAND}"
        "-DSOURCE_ROOT=${PROJECT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/check_boundary.cmake"
)
set_tests_properties(
    qindaqt.firewall-applet-boundary
    PROPERTIES LABELS "shell;firewall;pure;policy;hostile"
)

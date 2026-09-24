# SPDX-License-Identifier: GPL-3.0-or-later

# Network panel applet (docs/wiki/shell/network-applet.md, ADR-0258): pure
# projection and request rows, the controller over the public NetworkClient
# with the Settings route's fake Network1 transport, the compiled QML module
# offscreen with keyboard-only interaction, and the source boundary probe.

find_package(Qt6 6.11 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Svg Test)

set(network_applet_fixture_dirs
    "${PROJECT_SOURCE_DIR}/tests/apps/settings/network"
    "${CMAKE_CURRENT_SOURCE_DIR}")

function(qindaqt_add_network_applet_test target source test_name labels)
    # AGENT-NOTE: the fake transport is a Q_OBJECT header; listing it here is
    # what makes AUTOMOC generate its vtable.
    qt_add_executable(${target} ${source} network_applet_test_support.h
        "${PROJECT_SOURCE_DIR}/tests/apps/settings/network/network_settings_test_support.h")
    target_include_directories(${target} PRIVATE ${network_applet_fixture_dirs})
    target_link_libraries(${target} PRIVATE ${ARGN} Qt6::Test)
    qindaqt_enable_warnings(${target})
    add_test(NAME ${test_name} COMMAND ${target})
    set_tests_properties(${test_name} PROPERTIES LABELS "${labels}")
endfunction()

qindaqt_add_network_applet_test(
    qindaqt_network_applet_presentation_tests tst_network_applet_presentation.cpp
    qindaqt.network-applet-presentation "shell;network;pure"
    QindaQt::ShellNetworkApplet QindaQt::NetworkClient)
qindaqt_add_network_applet_test(
    qindaqt_network_applet_request_tests tst_network_request_state.cpp
    qindaqt.network-applet-request-state "shell;network;pure"
    QindaQt::ShellNetworkApplet QindaQt::NetworkClient)
qindaqt_add_network_applet_test(
    qindaqt_network_applet_controller_tests tst_network_applet_controller.cpp
    qindaqt.network-applet-controller "shell;network;runtime"
    QindaQt::ShellNetworkAppletRuntime)

qindaqt_add_network_applet_test(
    qindaqt_network_applet_qml_tests tst_network_applet_qml.cpp
    qindaqt.network-applet-offscreen "shell;network;runtime;offscreen;accessibility"
    QindaQt::ShellNetworkAppletRuntime
    qindaqt_shell_network_applet_runtimeplugin
    QindaQt::ShellIcons
    qindaqt_shell_iconsplugin
    QindaQt::TokensQml
    qindaqt_themes
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick)
target_compile_definitions(
    qindaqt_network_applet_qml_tests
    PRIVATE
        QINDAQT_NETWORK_APPLET_QML_IMPORT_PATH="${CMAKE_BINARY_DIR}/qml"
        QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
        QINDAQT_DESKTOP_CONTROLS_QML_IMPORT_PATH="${CMAKE_BINARY_DIR}/qml"
        QINDAQT_APPLET_ICON_FIXTURE_ROOT="${CMAKE_CURRENT_BINARY_DIR}/icon-resolution"
)
target_include_directories(qindaqt_network_applet_qml_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/tests/shell/desktop_controls")
add_dependencies(qindaqt_network_applet_qml_tests qindaqt_tokens_qmlplugin
                 qindaqt_controls_qmlplugin)
qt_import_qml_plugins(qindaqt_network_applet_qml_tests)
# AGENT-NOTE: QML warnings are failures here, as for every Settings QML row.
set_tests_properties(
    qindaqt.network-applet-offscreen
    PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_FATAL_WARNINGS=1"
)

add_test(
    NAME qindaqt.network-applet-boundary
    COMMAND
        "${CMAKE_COMMAND}"
        "-DSOURCE_ROOT=${PROJECT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/check_boundary.cmake"
)
set_tests_properties(
    qindaqt.network-applet-boundary
    PROPERTIES LABELS "shell;network;pure;policy;hostile"
)

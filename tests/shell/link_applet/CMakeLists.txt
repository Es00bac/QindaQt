# SPDX-License-Identifier: GPL-3.0-or-later

# Link panel applet (docs/wiki/shell/link-applet.md, ADR-0435): pure
# projection, the controller over the public LinkClient with a fake
# transport, the compiled QML module offscreen with keyboard-only
# interaction, and the source boundary probe.

find_package(Qt6 6.11 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Svg Test)

function(qindaqt_add_link_applet_test target source test_name labels)
    # AGENT-NOTE: the fake transport is a Q_OBJECT header; listing it here is
    # what makes AUTOMOC generate its vtable.
    qt_add_executable(${target} ${source} link_applet_test_support.h)
    target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(${target} PRIVATE ${ARGN} Qt6::Test)
    qindaqt_enable_warnings(${target})
    add_test(NAME ${test_name} COMMAND ${target})
    set_tests_properties(${test_name} PROPERTIES LABELS "${labels}")
endfunction()

qindaqt_add_link_applet_test(
    qindaqt_link_applet_presentation_tests tst_link_applet_presentation.cpp
    qindaqt.link-applet-presentation "shell;link;pure"
    QindaQt::ShellLinkApplet QindaQt::LinkClient)
qindaqt_add_link_applet_test(
    qindaqt_link_applet_controller_tests tst_link_applet_controller.cpp
    qindaqt.link-applet-controller "shell;link;runtime"
    QindaQt::ShellLinkAppletRuntime)

qindaqt_add_link_applet_test(
    qindaqt_link_applet_qml_tests tst_link_applet_qml.cpp
    qindaqt.link-applet-offscreen "shell;link;runtime;offscreen;accessibility"
    QindaQt::ShellLinkAppletRuntime
    qindaqt_shell_link_applet_runtimeplugin
    QindaQt::ShellIcons
    qindaqt_shell_iconsplugin
    QindaQt::TokensQml
    qindaqt_themes
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick)
target_compile_definitions(
    qindaqt_link_applet_qml_tests
    PRIVATE
        QINDAQT_LINK_APPLET_QML_IMPORT_PATH="${CMAKE_BINARY_DIR}/qml"
        QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
        QINDAQT_DESKTOP_CONTROLS_QML_IMPORT_PATH="${CMAKE_BINARY_DIR}/qml"
        QINDAQT_LINK_APPLET_ICON_FIXTURE_ROOT="${CMAKE_CURRENT_BINARY_DIR}/icon-resolution"
)
target_include_directories(qindaqt_link_applet_qml_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/tests/shell/desktop_controls")
add_dependencies(qindaqt_link_applet_qml_tests qindaqt_tokens_qmlplugin
                 qindaqt_controls_qmlplugin)
qt_import_qml_plugins(qindaqt_link_applet_qml_tests)
# AGENT-NOTE: QML warnings are failures here, as for every Settings QML row.
set_tests_properties(
    qindaqt.link-applet-offscreen
    PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_FATAL_WARNINGS=1"
)

add_test(
    NAME qindaqt.link-applet-boundary
    COMMAND
        "${CMAKE_COMMAND}"
        "-DSOURCE_ROOT=${PROJECT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/check_boundary.cmake"
)
set_tests_properties(
    qindaqt.link-applet-boundary
    PROPERTIES LABELS "shell;link;pure;policy;hostile"
)

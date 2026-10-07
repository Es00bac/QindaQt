# SPDX-License-Identifier: GPL-3.0-or-later
find_package(Qt6 6.11 REQUIRED COMPONENTS Test Gui Qml Quick)
qt_add_executable(qindaqt_agent_usage_applet_tests tst_agent_usage.cpp
    "${PROJECT_SOURCE_DIR}/src/shell/runtime/agentusageappletcomposition.cpp")
target_include_directories(qindaqt_agent_usage_applet_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/src/shell/runtime")
target_link_libraries(qindaqt_agent_usage_applet_tests PRIVATE
    QindaQt::ShellAgentUsageAppletRuntime QindaQt::AppletRuntime Qt6::Test)
target_compile_definitions(qindaqt_agent_usage_applet_tests PRIVATE QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
qindaqt_enable_warnings(qindaqt_agent_usage_applet_tests)
add_test(NAME qindaqt.agent-usage-applet COMMAND qindaqt_agent_usage_applet_tests)
set_tests_properties(qindaqt.agent-usage-applet PROPERTIES LABELS "shell;agent-usage;pure;policy")
qt_add_executable(qindaqt_agent_usage_applet_qml_tests tst_agent_usage_qml.cpp)
target_link_libraries(qindaqt_agent_usage_applet_qml_tests PRIVATE
    QindaQt::ShellAgentUsageAppletRuntime qindaqt_shell_agent_usage_applet_runtimeplugin
    QindaQt::ShellIcons qindaqt_shell_iconsplugin QindaQt::TokensQml qindaqt_themes
    Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Test)
target_include_directories(qindaqt_agent_usage_applet_qml_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/tests/shell/desktop_controls")
target_compile_definitions(qindaqt_agent_usage_applet_qml_tests PRIVATE
    QINDAQT_DESKTOP_CONTROLS_QML_IMPORT_PATH="${CMAKE_BINARY_DIR}/qml"
    QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
    QINDAQT_APPLET_ICON_FIXTURE_ROOT="${CMAKE_CURRENT_BINARY_DIR}/icon-resolution")
add_dependencies(qindaqt_agent_usage_applet_qml_tests qindaqt_controls_qmlplugin qindaqt_tokens_qmlplugin)
qt_import_qml_plugins(qindaqt_agent_usage_applet_qml_tests)
qindaqt_enable_warnings(qindaqt_agent_usage_applet_qml_tests)
add_test(NAME qindaqt.agent-usage-applet-offscreen COMMAND qindaqt_agent_usage_applet_qml_tests)
set_tests_properties(qindaqt.agent-usage-applet-offscreen PROPERTIES
    LABELS "shell;agent-usage;offscreen;accessibility"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_FATAL_WARNINGS=1")

add_test(NAME qindaqt.agent-usage-applet-boundary
    COMMAND "${CMAKE_COMMAND}" "-DSOURCE_ROOT=${PROJECT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/check_boundary.cmake")
set_tests_properties(qindaqt.agent-usage-applet-boundary PROPERTIES LABELS "shell;agent-usage;policy;boundary")

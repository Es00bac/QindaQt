# SPDX-License-Identifier: GPL-3.0-or-later
qt_add_library(qindaqt_shell_agent_usage_applet_runtime STATIC)
add_library(QindaQt::ShellAgentUsageAppletRuntime ALIAS qindaqt_shell_agent_usage_applet_runtime)
target_sources(qindaqt_shell_agent_usage_applet_runtime PRIVATE
    src/agent_usage_presentation.cpp src/agent_usage_presentation.h
    src/agent_usage_applet_controller.cpp src/agent_usage_applet_controller.h)
qt_add_qml_module(qindaqt_shell_agent_usage_applet_runtime
    URI QindaQt.Shell.AgentUsageApplet VERSION 1.0
    OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/qml/QindaQt/Shell/AgentUsageApplet"
    QML_FILES qml/AgentUsageApplet.qml qml/AgentUsageProviderRow.qml
    DEPENDENCIES TARGET qindaqt_shell_icons)
target_include_directories(qindaqt_shell_agent_usage_applet_runtime PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(qindaqt_shell_agent_usage_applet_runtime PUBLIC
    QindaQt::AgentUsage Qt6::Core Qt6::Qml Qt6::Quick Qt6::QuickControls2 QindaQt::ShellIcons)
target_compile_features(qindaqt_shell_agent_usage_applet_runtime PUBLIC cxx_std_20)
set_target_properties(qindaqt_shell_agent_usage_applet_runtime PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_shell_agent_usage_applet_runtime)
add_dependencies(qindaqt_shell_agent_usage_applet_runtime qindaqt_controls_qmlplugin)

# SPDX-License-Identifier: GPL-3.0-or-later
qt_add_library(qindaqt_system_monitor_ui STATIC)

target_sources(qindaqt_system_monitor_ui
    PRIVATE
        metric_chart.cpp
        monitor_controller.cpp
        system_monitor_appearance.cpp
        system_monitor_window.cpp
        system_monitor_window_presentation.cpp
        system_monitor_window_processes.cpp
        ../app_shell/system_monitor_action_catalog.cpp
        ../app_shell/system_monitor_app_shell_bridge.cpp
    PUBLIC
        metric_chart.h
        monitor_controller.h
        system_monitor_appearance.h
        system_monitor_window.h
        ../app_shell/system_monitor_action_catalog.h
        ../app_shell/system_monitor_app_shell_bridge.h
)

target_include_directories(qindaqt_system_monitor_ui PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_link_libraries(qindaqt_system_monitor_ui
    PUBLIC
        QindaQt::AppShell
        QindaQt::AppShellMenuExport
        QindaQt::DesignTokens
        QindaQt::SystemMonitorCore
        qindaqt_themes
        Qt6::Concurrent
        Qt6::Core
        Qt6::DBus
        Qt6::Gui
        Qt6::Widgets
)
target_compile_features(qindaqt_system_monitor_ui PUBLIC cxx_std_20)
set_target_properties(qindaqt_system_monitor_ui PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_system_monitor_ui)

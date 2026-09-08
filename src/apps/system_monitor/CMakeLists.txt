# SPDX-License-Identifier: GPL-3.0-or-later
find_package(Qt6 REQUIRED COMPONENTS Concurrent DBus)

add_subdirectory(core)
add_subdirectory(hardware)
add_subdirectory(ui)

qt_add_executable(qindaqt-system-monitor main.cpp)
target_link_libraries(qindaqt-system-monitor
    PRIVATE
        qindaqt_system_monitor_ui
        QindaQt::SystemMonitorCore
        QindaQt::SystemMonitorHardware
        QindaQt::AppAppearance
        QindaQt::AppShellMenuExport
        QindaQt::FontDiscovery
        qindaqt_settings_client
        Qt6::DBus
)
target_compile_features(qindaqt-system-monitor PRIVATE cxx_std_20)
set_target_properties(qindaqt-system-monitor PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt-system-monitor)

# The monitor is independently installable but AppShell lives under Qt's QML
# tree. Preserve the relative lookup used by the other first-party Widgets apps.
set(qindaqt_system_monitor_bindir "${CMAKE_INSTALL_BINDIR}")
cmake_path(ABSOLUTE_PATH qindaqt_system_monitor_bindir BASE_DIRECTORY "${CMAKE_INSTALL_PREFIX}" NORMALIZE OUTPUT_VARIABLE qindaqt_system_monitor_bindir_absolute)
set(qindaqt_system_monitor_qmldir "${QT6_INSTALL_QML}")
cmake_path(ABSOLUTE_PATH qindaqt_system_monitor_qmldir BASE_DIRECTORY "${CMAKE_INSTALL_PREFIX}" NORMALIZE OUTPUT_VARIABLE qindaqt_system_monitor_qmldir_absolute)
cmake_path(RELATIVE_PATH qindaqt_system_monitor_qmldir_absolute BASE_DIRECTORY "${qindaqt_system_monitor_bindir_absolute}" OUTPUT_VARIABLE qindaqt_system_monitor_qml_relative_path)
set_target_properties(qindaqt-system-monitor PROPERTIES INSTALL_RPATH "$ORIGIN/${qindaqt_system_monitor_qml_relative_path}/QindaQt/AppShell")

install(TARGETS qindaqt-system-monitor RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT SystemMonitor)
install(FILES org.qindaqt.SystemMonitor.desktop DESTINATION "${CMAKE_INSTALL_DATADIR}/applications" COMPONENT SystemMonitor)
install(FILES icons/org.qindaqt.SystemMonitor.svg DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps" COMPONENT SystemMonitor)

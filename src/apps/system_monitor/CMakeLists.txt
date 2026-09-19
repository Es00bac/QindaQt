# SPDX-License-Identifier: GPL-3.0-or-later
find_package(Qt6 REQUIRED COMPONENTS Concurrent Gui Qml Quick)

add_subdirectory(core)
add_subdirectory(hardware)

# The view model is a library, not part of the executable, so tests can link
# it without dragging in a QML engine or a window.
qt_add_library(qindaqt_system_monitor_view STATIC
    ui/monitor_facade.cpp
    ui/monitor_facade.h
    ui/process_table_model.cpp
    ui/process_table_model.h
)
add_library(QindaQt::SystemMonitorView ALIAS qindaqt_system_monitor_view)
target_include_directories(qindaqt_system_monitor_view PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/ui")
target_link_libraries(qindaqt_system_monitor_view
    PUBLIC Qt6::Core Qt6::Gui
    PRIVATE QindaQt::SystemMonitorHardware Qt6::Concurrent
)
target_compile_features(qindaqt_system_monitor_view PUBLIC cxx_std_20)
set_target_properties(qindaqt_system_monitor_view
    PROPERTIES CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON AUTOUIC OFF)
qindaqt_enable_warnings(qindaqt_system_monitor_view)

qt_add_executable(qindaqt-system-monitor main.cpp)

# AGENT-CONTRACT: the interface is QML built on QindaTK (dev-libs/qindatk),
# which supplies the telemetry primitives -- Tk.Graph, Tk.Meter, Tk.Sparkline,
# Tk.DataTable -- and the QindaTK.QindaQt bridge that feeds the desktop's
# QST-1 tokens into the toolkit theme. Nothing links QindaTK: the module is on
# Qt's QML import path and this application uses none of its C++ API.
qt_add_qml_module(qindaqt-system-monitor
    URI QindaQt.SystemMonitor
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/Dashboard.qml
        qml/panels/CpuPanel.qml
        qml/panels/MemoryPanel.qml
        qml/panels/DisksPanel.qml
        qml/panels/NetworkPanel.qml
        qml/panels/ProcessPanel.qml
        qml/panels/HardwarePanel.qml
        qml/parts/MonitorPanel.qml
        qml/parts/Readout.qml
        qml/parts/MeterRow.qml
        qml/parts/TraceGraph.qml
        qml/parts/ProcessDetails.qml
)

target_link_libraries(qindaqt-system-monitor
    PRIVATE
        QindaQt::SystemMonitorCore
        QindaQt::SystemMonitorHardware
        QindaQt::SystemMonitorView
        Qt6::Concurrent
        Qt6::Gui
        Qt6::Quick
)
target_compile_features(qindaqt-system-monitor PRIVATE cxx_std_20)
set_target_properties(qindaqt-system-monitor PROPERTIES CXX_EXTENSIONS OFF AUTOUIC OFF)
qindaqt_enable_warnings(qindaqt-system-monitor)

install(TARGETS qindaqt-system-monitor
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT SystemMonitor)
install(FILES org.qindaqt.SystemMonitor.desktop
    DESTINATION "${CMAKE_INSTALL_DATADIR}/applications" COMPONENT SystemMonitor)
install(FILES icons/org.qindaqt.SystemMonitor.svg
    DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps" COMPONENT SystemMonitor)

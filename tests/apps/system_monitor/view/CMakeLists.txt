# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_executable(qindaqt_system_monitor_view_tests tst_process_table_model.cpp)
target_link_libraries(qindaqt_system_monitor_view_tests
    PRIVATE QindaQt::SystemMonitorView Qt6::Test)
target_compile_features(qindaqt_system_monitor_view_tests PRIVATE cxx_std_20)
set_target_properties(qindaqt_system_monitor_view_tests PROPERTIES CXX_EXTENSIONS OFF AUTOUIC OFF)
qindaqt_enable_warnings(qindaqt_system_monitor_view_tests)
add_test(NAME qindaqt.system-monitor-view COMMAND qindaqt_system_monitor_view_tests)
set_tests_properties(qindaqt.system-monitor-view PROPERTIES LABELS "system-monitor")

qt_add_executable(qindaqt_system_monitor_facade_tests tst_monitor_facade.cpp)
target_link_libraries(qindaqt_system_monitor_facade_tests
    PRIVATE QindaQt::SystemMonitorView Qt6::Test)
target_compile_features(qindaqt_system_monitor_facade_tests PRIVATE cxx_std_20)
set_target_properties(qindaqt_system_monitor_facade_tests PROPERTIES CXX_EXTENSIONS OFF AUTOUIC OFF)
qindaqt_enable_warnings(qindaqt_system_monitor_facade_tests)
add_test(NAME qindaqt.system-monitor-facade COMMAND qindaqt_system_monitor_facade_tests)
set_tests_properties(qindaqt.system-monitor-facade PROPERTIES LABELS "system-monitor")

qt_add_executable(qindaqt_system_monitor_actions_tests tst_system_monitor_actions.cpp)
target_link_libraries(qindaqt_system_monitor_actions_tests
    PRIVATE QindaQt::SystemMonitorView Qt6::Test)
target_compile_features(qindaqt_system_monitor_actions_tests PRIVATE cxx_std_20)
set_target_properties(qindaqt_system_monitor_actions_tests
    PROPERTIES CXX_EXTENSIONS OFF AUTOUIC OFF)
qindaqt_enable_warnings(qindaqt_system_monitor_actions_tests)
add_test(NAME qindaqt.system-monitor-actions COMMAND qindaqt_system_monitor_actions_tests)
set_tests_properties(qindaqt.system-monitor-actions PROPERTIES LABELS "system-monitor")

# AGENT-GUARD: these rows construct a QGuiApplication, so they must name a
# platform. Without this they default to xcb and abort before their first
# assertion on any machine with no display -- a headless runner, or a
# developer on a VT -- which reads as a real failure and is not one. Every
# one of them passes under offscreen. Keep this list in step with the rows
# above; a new GUI row added without it fails the same way.
set_tests_properties(
    qindaqt.system-monitor-actions
    qindaqt.system-monitor-facade
    qindaqt.system-monitor-view
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

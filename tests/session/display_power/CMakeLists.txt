qt_add_executable(qindaqt_scoped_display_power_tests tst_scoped_display_power.cpp)
target_link_libraries(qindaqt_scoped_display_power_tests PRIVATE QindaQt::DisplayPower
    qindaqt_compositor_names Qt6::Test)
qindaqt_enable_warnings(qindaqt_scoped_display_power_tests)
add_test(NAME qindaqt.scoped-display-power COMMAND qindaqt_scoped_display_power_tests)
set_tests_properties(qindaqt.scoped-display-power PROPERTIES LABELS "session;power;dbus;private-fixture" TIMEOUT 30 RUN_SERIAL TRUE)

qt_add_executable(qindaqt_screen_power_service_tests tst_screen_power_service.cpp)
target_link_libraries(qindaqt_screen_power_service_tests PRIVATE QindaQt::DisplayPower QindaQt::NativeSleep qindaqt_compositor_names Qt6::Test)
target_compile_definitions(qindaqt_screen_power_service_tests PRIVATE QINDAQT_DBUS_DAEMON_EXECUTABLE="${DBUS_DAEMON_EXECUTABLE}")
qindaqt_enable_warnings(qindaqt_screen_power_service_tests)
add_test(NAME qindaqt.screen-power-service COMMAND qindaqt_screen_power_service_tests)
set_tests_properties(qindaqt.screen-power-service PROPERTIES LABELS "session;power;dbus;private-fixture;lock" TIMEOUT 30 RUN_SERIAL TRUE)

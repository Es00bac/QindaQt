qt_add_executable(qindaqt_scoped_display_power_tests tst_scoped_display_power.cpp)
target_link_libraries(qindaqt_scoped_display_power_tests PRIVATE QindaQt::DisplayPower
    qindaqt_compositor_names Qt6::Test)
qindaqt_enable_warnings(qindaqt_scoped_display_power_tests)
add_test(NAME qindaqt.scoped-display-power COMMAND qindaqt_scoped_display_power_tests)
set_tests_properties(qindaqt.scoped-display-power PROPERTIES LABELS "session;power;dbus;private-fixture" TIMEOUT 30 RUN_SERIAL TRUE)

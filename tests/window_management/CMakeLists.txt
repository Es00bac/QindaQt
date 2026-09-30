foreach(behavior IN ITEMS geometry_codec context_controller)
    add_executable(qindaqt_window_management_${behavior}_tests tst_${behavior}.cpp)
    target_link_libraries(qindaqt_window_management_${behavior}_tests PRIVATE QindaQt::WindowManagement Qt6::Test)
    target_compile_features(qindaqt_window_management_${behavior}_tests PRIVATE cxx_std_20)
    qindaqt_enable_warnings(qindaqt_window_management_${behavior}_tests)
    add_test(NAME qindaqt.window-management-${behavior} COMMAND qindaqt_window_management_${behavior}_tests)
endforeach()

add_executable(qindaqt_window_management_qt_endpoint_tests tst_qt_endpoint.cpp)
target_link_libraries(qindaqt_window_management_qt_endpoint_tests PRIVATE QindaQt::WindowManagementTransport Qt6::Test)
target_compile_features(qindaqt_window_management_qt_endpoint_tests PRIVATE cxx_std_20)
qindaqt_enable_warnings(qindaqt_window_management_qt_endpoint_tests)
find_program(QINDAQT_COMMAND_DBUS_RUN_SESSION dbus-run-session REQUIRED)
add_test(NAME qindaqt.window-management-qt_endpoint
    COMMAND ${QINDAQT_COMMAND_DBUS_RUN_SESSION} -- $<TARGET_FILE:qindaqt_window_management_qt_endpoint_tests>)

# Private native probe, not a live-session test or an installed application.
find_package(LayerShellQt 6.6.6 REQUIRED)
add_executable(qindaqt_window_management_native_probe native_probe.cpp)
target_link_libraries(qindaqt_window_management_native_probe PRIVATE QindaQt::WindowManagementTransport QindaQt::ApplicationWindowClient Qt6::Test Qt6::Widgets LayerShellQt::Interface)
target_compile_features(qindaqt_window_management_native_probe PRIVATE cxx_std_20)
qindaqt_enable_warnings(qindaqt_window_management_native_probe)

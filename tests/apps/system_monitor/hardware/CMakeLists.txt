# SPDX-License-Identifier: GPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Core Test)
set(CMAKE_AUTOMOC ON)

qt_add_executable(qindaqt_system_monitor_hardware_tests tst_hardware_sampler.cpp)
target_link_libraries(
    qindaqt_system_monitor_hardware_tests
    PRIVATE QindaQt::SystemMonitorHardware Qt6::Core Qt6::Test
)
target_compile_features(qindaqt_system_monitor_hardware_tests PRIVATE cxx_std_20)
set_target_properties(qindaqt_system_monitor_hardware_tests PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_system_monitor_hardware_tests)
add_test(
    NAME qindaqt.system-monitor-hardware
    COMMAND qindaqt_system_monitor_hardware_tests
)
set(QINDAQT_SYSTEM_MONITOR_TEST_TMP "${CMAKE_CURRENT_BINARY_DIR}/temporary")
file(MAKE_DIRECTORY "${QINDAQT_SYSTEM_MONITOR_TEST_TMP}")
set_tests_properties(
    qindaqt.system-monitor-hardware
    PROPERTIES
        LABELS "system-monitor;hardware"
        ENVIRONMENT "TMPDIR=${QINDAQT_SYSTEM_MONITOR_TEST_TMP}"
)

# SPDX-License-Identifier: GPL-3.0-or-later

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.28)
    project(qindaqt-system-monitor-core-tests LANGUAGES CXX)
    set(CMAKE_AUTOMOC ON)
    enable_testing()
    find_package(Qt6 6.11 REQUIRED COMPONENTS Core Concurrent Test)
    add_subdirectory(
        "${CMAKE_CURRENT_LIST_DIR}/../../../../src/apps/system_monitor/core"
        "${CMAKE_CURRENT_BINARY_DIR}/system-monitor-core"
    )
endif()

qt_add_executable(qindaqt_system_monitor_procfs_tests tst_procfs_reader.cpp)
target_link_libraries(
    qindaqt_system_monitor_procfs_tests PRIVATE QindaQt::SystemMonitorCore Qt6::Test
)
target_compile_features(qindaqt_system_monitor_procfs_tests PRIVATE cxx_std_20)

qt_add_executable(qindaqt_system_monitor_sampling_tests tst_sample_collector.cpp)
target_link_libraries(
    qindaqt_system_monitor_sampling_tests PRIVATE QindaQt::SystemMonitorCore Qt6::Test
)
target_compile_features(qindaqt_system_monitor_sampling_tests PRIVATE cxx_std_20)

qt_add_executable(qindaqt_system_monitor_engine_tests tst_monitor_engine.cpp)
target_link_libraries(
    qindaqt_system_monitor_engine_tests PRIVATE QindaQt::SystemMonitorCore Qt6::Test
)
target_compile_features(qindaqt_system_monitor_engine_tests PRIVATE cxx_std_20)

foreach(target IN ITEMS
        qindaqt_system_monitor_procfs_tests
        qindaqt_system_monitor_sampling_tests
        qindaqt_system_monitor_engine_tests)
    set_target_properties(${target} PROPERTIES CXX_EXTENSIONS OFF)
    if(COMMAND qindaqt_enable_warnings)
        qindaqt_enable_warnings(${target})
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
endforeach()

add_test(NAME qindaqt.system-monitor-procfs COMMAND qindaqt_system_monitor_procfs_tests)
add_test(NAME qindaqt.system-monitor-sampling COMMAND qindaqt_system_monitor_sampling_tests)
add_test(NAME qindaqt.system-monitor-engine COMMAND qindaqt_system_monitor_engine_tests)
set_tests_properties(
    qindaqt.system-monitor-procfs qindaqt.system-monitor-sampling
    qindaqt.system-monitor-engine
    PROPERTIES LABELS "apps;system-monitor;core" TIMEOUT 20
)

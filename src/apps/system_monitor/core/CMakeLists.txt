# SPDX-License-Identifier: GPL-3.0-or-later

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.28)
    project(qindaqt-system-monitor-core LANGUAGES CXX)
    set(CMAKE_AUTOMOC ON)
endif()

find_package(Qt6 6.11 REQUIRED COMPONENTS Core Concurrent)

qt_add_library(qindaqt_system_monitor_core STATIC)
target_sources(
    qindaqt_system_monitor_core
    PRIVATE
        monitor_engine.cpp
        process_actions.cpp
        process_model.cpp
        procfs_reader.cpp
        sample_collector.cpp
        process_actions.h
        process_model.h
        procfs_reader.h
        sample_collector.h
        sample_types.h
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}"
        FILES monitor_engine.h
)
target_include_directories(
    qindaqt_system_monitor_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}"
)
target_link_libraries(
    qindaqt_system_monitor_core PUBLIC Qt6::Core PRIVATE Qt6::Concurrent
)
target_compile_features(qindaqt_system_monitor_core PUBLIC cxx_std_20)
set_target_properties(
    qindaqt_system_monitor_core
    PROPERTIES CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON
)
if(COMMAND qindaqt_enable_warnings)
    qindaqt_enable_warnings(qindaqt_system_monitor_core)
else()
    target_compile_options(
        qindaqt_system_monitor_core PRIVATE -Wall -Wextra -Wpedantic -Werror
    )
endif()
add_library(QindaQt::SystemMonitorCore ALIAS qindaqt_system_monitor_core)

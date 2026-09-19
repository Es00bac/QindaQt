# SPDX-License-Identifier: GPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Core)

qt_add_library(qindaqt_system_monitor_hardware STATIC)
add_library(QindaQt::SystemMonitorHardware ALIAS qindaqt_system_monitor_hardware)
target_sources(
    qindaqt_system_monitor_hardware
    PRIVATE hardware_sampler.cpp nvidia_telemetry.cpp
    PUBLIC FILE_SET HEADERS BASE_DIRS . FILES hardware_sampler.h nvidia_telemetry.h
)
target_include_directories(
    qindaqt_system_monitor_hardware
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
)
target_link_libraries(qindaqt_system_monitor_hardware PUBLIC Qt6::Core)
target_compile_features(qindaqt_system_monitor_hardware PUBLIC cxx_std_20)
set_target_properties(qindaqt_system_monitor_hardware PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_system_monitor_hardware)

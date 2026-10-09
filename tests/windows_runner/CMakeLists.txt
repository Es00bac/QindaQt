# SPDX-License-Identifier: LGPL-3.0-or-later
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.25)
    project(QindaQtWindowsRunnerTests LANGUAGES CXX)
    include(CTest)
    include(GNUInstallDirs)
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake")
    include(QindaQtCompiler)
    find_package(Qt6 6.11 REQUIRED COMPONENTS Core Test)
    qt_standard_project_setup(REQUIRES 6.11)
    add_subdirectory("../../src/windows_runner" "${CMAKE_BINARY_DIR}/src/windows_runner")
endif()

foreach(behavior IN ITEMS launch_plan windows_runner)
    qt_add_executable(qindaqt_windows_${behavior}_tests tst_${behavior}.cpp)
    target_link_libraries(qindaqt_windows_${behavior}_tests PRIVATE QindaQt::WindowsRunner Qt6::Test)
    qindaqt_enable_warnings(qindaqt_windows_${behavior}_tests)
    add_test(NAME windows-runner.${behavior} COMMAND qindaqt_windows_${behavior}_tests)
    set_tests_properties(windows-runner.${behavior} PROPERTIES
        TIMEOUT 30 LABELS "windows;foreign-apps;focused"
        ENVIRONMENT_MODIFICATION "DISPLAY=unset:;WAYLAND_DISPLAY=unset:;DBUS_SESSION_BUS_ADDRESS=unset:")
endforeach()

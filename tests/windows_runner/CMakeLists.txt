# SPDX-License-Identifier: LGPL-3.0-or-later
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.25)
    project(QindaQtWindowsRunnerTests LANGUAGES CXX)
    include(CTest)
    include(GNUInstallDirs)
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake")
    include(QindaQtCompiler)
    find_package(Qt6 6.11 REQUIRED COMPONENTS Core Gui Test)
    qt_standard_project_setup(REQUIRES 6.11)
    set(QINDAQT_LAUNCHER_PURE_ONLY ON)
    add_subdirectory("../../src/shell/launcher" "${CMAKE_BINARY_DIR}/src/shell/launcher")
    add_subdirectory("../../src/application_catalog" "${CMAKE_BINARY_DIR}/src/application_catalog")
    add_subdirectory("../../src/windows_runner" "${CMAKE_BINARY_DIR}/src/windows_runner")
endif()

foreach(behavior IN ITEMS launch_plan windows_runner catalog_binding executable_file)
    qt_add_executable(qindaqt_windows_${behavior}_tests tst_${behavior}.cpp)
    target_link_libraries(qindaqt_windows_${behavior}_tests PRIVATE QindaQt::WindowsRunner Qt6::Test)
    qindaqt_enable_warnings(qindaqt_windows_${behavior}_tests)
    add_test(NAME windows-runner.${behavior} COMMAND qindaqt_windows_${behavior}_tests)
    set_tests_properties(windows-runner.${behavior} PROPERTIES
        TIMEOUT 30 LABELS "windows;foreign-apps;focused"
        ENVIRONMENT_MODIFICATION "DISPLAY=unset:;WAYLAND_DISPLAY=unset:;DBUS_SESSION_BUS_ADDRESS=unset:")
endforeach()

qt_add_executable(qindaqt_windows_file_manager_open_smoke
    tst_file_manager_windows_open.cpp
    "${CMAKE_CURRENT_SOURCE_DIR}/../../src/apps/file_manager/model/launch_intent.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../src/apps/file_manager/model/open_with_launcher.cpp")
target_include_directories(qindaqt_windows_file_manager_open_smoke PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../../src/apps/file_manager")
target_link_libraries(qindaqt_windows_file_manager_open_smoke PRIVATE
    QindaQt::WindowsRunner QindaQt::ApplicationCatalog Qt6::Gui Qt6::Test)
qindaqt_enable_warnings(qindaqt_windows_file_manager_open_smoke)
add_test(NAME windows-runner.file-manager-open-smoke COMMAND qindaqt_windows_file_manager_open_smoke)
set_tests_properties(windows-runner.file-manager-open-smoke PROPERTIES
    TIMEOUT 30 LABELS "windows;file-manager;focused"
    ENVIRONMENT_MODIFICATION "DISPLAY=unset:;WAYLAND_DISPLAY=unset:;DBUS_SESSION_BUS_ADDRESS=unset:")

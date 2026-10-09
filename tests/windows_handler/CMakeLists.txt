# SPDX-License-Identifier: GPL-3.0-or-later
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.25)
    project(QindaQtWindowsHandlerTests LANGUAGES CXX)
    include(CTest)
    include(GNUInstallDirs)
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake")
    include(QindaQtCompiler)
    find_package(Qt6 6.11 REQUIRED COMPONENTS Core Gui Widgets Concurrent Test)
    qt_standard_project_setup(REQUIRES 6.11)
    set(QINDAQT_LAUNCHER_PURE_ONLY ON)
    add_subdirectory("../../src/shell/launcher" "${CMAKE_BINARY_DIR}/src/shell/launcher")
    add_subdirectory("../../src/windows_runner" "${CMAKE_BINARY_DIR}/src/windows_runner")
    add_subdirectory("../../src/apps/windows_runner" "${CMAKE_BINARY_DIR}/src/apps/windows_runner")
endif()
qt_add_executable(qindaqt_windows_handler_source_smoke tst_user_profile.cpp)
target_include_directories(qindaqt_windows_handler_source_smoke PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../../src/apps/windows_runner"
    "${CMAKE_CURRENT_SOURCE_DIR}/../windows_runner")
target_link_libraries(qindaqt_windows_handler_source_smoke PRIVATE qindaqt_windows_handler_support Qt6::Test)
qindaqt_enable_warnings(qindaqt_windows_handler_source_smoke)
add_test(NAME windows-handler.source-smoke COMMAND qindaqt_windows_handler_source_smoke)
set_tests_properties(windows-handler.source-smoke PROPERTIES TIMEOUT 30 LABELS "windows;handler;focused"
    ENVIRONMENT_MODIFICATION "DISPLAY=unset:;WAYLAND_DISPLAY=unset:;DBUS_SESSION_BUS_ADDRESS=unset:")

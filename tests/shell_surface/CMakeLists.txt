# SPDX-License-Identifier: GPL-3.0-or-later

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.25)
    project(QindaQtShellSurfaceTests LANGUAGES CXX)

    include(CTest)
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake")
    include(QindaQtCompiler)
    find_package(ECM 6.0 REQUIRED NO_MODULE)
    list(APPEND CMAKE_MODULE_PATH "${ECM_MODULE_PATH}")
    include(KDEInstallDirs6)
    find_package(Qt6 6.11 REQUIRED COMPONENTS Core Gui Quick Test)
    qt_standard_project_setup(REQUIRES 6.11)

    add_subdirectory("../../src/profiles" "${CMAKE_BINARY_DIR}/src/profiles")
    add_subdirectory("../../src/shell_layout" "${CMAKE_BINARY_DIR}/src/shell_layout")
    add_subdirectory("../../src/shell_surface" "${CMAKE_BINARY_DIR}/src/shell_surface")
endif()

function(qindaqt_add_shell_surface_test target source test_name)
    qt_add_executable(${target} ${source})
    target_link_libraries(${target} PRIVATE QindaQt::ShellSurface Qt6::Test)
    qindaqt_enable_warnings(${target})
    add_test(NAME ${test_name} COMMAND ${target})
endfunction()

qindaqt_add_shell_surface_test(
    qindaqt_panel_surface_configuration_tests
    tst_panel_surface_configuration.cpp
    qindaqt.shell-surface-configuration
)
qindaqt_add_shell_surface_test(
    qindaqt_panel_surface_controller_tests
    tst_panel_surface_controller.cpp
    qindaqt.shell-surface-controller
)
qindaqt_add_shell_surface_test(
    qindaqt_panel_surface_runtime_planner_tests
    tst_panel_surface_runtime_planner.cpp
    qindaqt.shell-surface-runtime-planner
)

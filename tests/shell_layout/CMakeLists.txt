# SPDX-License-Identifier: GPL-3.0-or-later

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.25)
    project(QindaQtShellLayoutTests LANGUAGES CXX)

    include(CTest)
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake")
    include(QindaQtCompiler)
    find_package(ECM 6.0 REQUIRED NO_MODULE)
    list(APPEND CMAKE_MODULE_PATH "${ECM_MODULE_PATH}")
    include(KDEInstallDirs6)
    find_package(Qt6 6.11 REQUIRED COMPONENTS Core Test)
    qt_standard_project_setup(REQUIRES 6.11)

    add_subdirectory("../../src/profiles" "${CMAKE_BINARY_DIR}/src/profiles")
    add_subdirectory("../../src/shell_layout" "${CMAKE_BINARY_DIR}/src/shell_layout")
endif()

qt_add_executable(
    qindaqt_panel_layout_geometry_tests
    tst_panel_layout_geometry.cpp
)
target_link_libraries(
    qindaqt_panel_layout_geometry_tests
    PRIVATE QindaQt::ShellLayout Qt6::Test
)
qindaqt_enable_warnings(qindaqt_panel_layout_geometry_tests)
add_test(
    NAME qindaqt.shell-layout-geometry
    COMMAND qindaqt_panel_layout_geometry_tests
)

qt_add_executable(
    qindaqt_panel_layout_validation_tests
    tst_panel_layout_validation.cpp
)
target_link_libraries(
    qindaqt_panel_layout_validation_tests
    PRIVATE QindaQt::ShellLayout Qt6::Test
)
qindaqt_enable_warnings(qindaqt_panel_layout_validation_tests)
add_test(
    NAME qindaqt.shell-layout-validation
    COMMAND qindaqt_panel_layout_validation_tests
)

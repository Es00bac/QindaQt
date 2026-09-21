# SPDX-License-Identifier: GPL-3.0-or-later

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.25)
    project(QindaQtHybridGatherTests LANGUAGES CXX)

    include(CTest)
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake")
    include(QindaQtCompiler)
    find_package(Qt6 6.11 REQUIRED COMPONENTS Core Test)
    qt_standard_project_setup(REQUIRES 6.11)

    # AGENT-NOTE: the narrow entry point, so the gather geometry can be
    # qualified without configuring the whole desktop.
    add_subdirectory(
        "../../src/hybrid_gather"
        "${CMAKE_BINARY_DIR}/src/hybrid_gather"
    )
endif()

qt_add_executable(qindaqt_gather_layout_tests tst_gather_layout.cpp)
target_link_libraries(
    qindaqt_gather_layout_tests
    PRIVATE QindaQt::HybridGather Qt6::Test
)
set_target_properties(
    qindaqt_gather_layout_tests PROPERTIES AUTOMOC ON CXX_EXTENSIONS OFF
)
qindaqt_enable_warnings(qindaqt_gather_layout_tests)
add_test(NAME qindaqt.gather-layout COMMAND qindaqt_gather_layout_tests)
set_tests_properties(
    qindaqt.gather-layout PROPERTIES LABELS "unit;pure;compositor"
)

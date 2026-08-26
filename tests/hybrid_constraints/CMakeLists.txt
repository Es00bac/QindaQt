# SPDX-License-Identifier: GPL-3.0-or-later

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.25)
    project(QindaQtHybridConstraintsTests LANGUAGES CXX)

    include(CTest)
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake")
    include(QindaQtCompiler)
    find_package(Qt6 6.11 REQUIRED COMPONENTS Core Test)
    qt_standard_project_setup(REQUIRES 6.11)

    # AGENT-NOTE: This narrow entry point lets the module owner qualify an
    # isolated slice before editing the repository-wide coordination files.
    add_subdirectory("../../src/core" "${CMAKE_BINARY_DIR}/src/core")
    add_subdirectory(
        "../../src/hybrid_constraints"
        "${CMAKE_BINARY_DIR}/src/hybrid_constraints"
    )
endif()

qt_add_executable(qindaqt_constraint_solver_tests tst_constraint_solver.cpp)
target_link_libraries(
    qindaqt_constraint_solver_tests
    PRIVATE QindaQt::HybridConstraints Qt6::Test
)
if(COMMAND qindaqt_enable_warnings)
    qindaqt_enable_warnings(qindaqt_constraint_solver_tests)
endif()
add_test(NAME qindaqt.hybrid-constraint-solver COMMAND qindaqt_constraint_solver_tests)

qt_add_executable(qindaqt_window_restore_state_tests tst_window_restore_state.cpp)
target_link_libraries(
    qindaqt_window_restore_state_tests
    PRIVATE QindaQt::HybridConstraints Qt6::Test
)
if(COMMAND qindaqt_enable_warnings)
    qindaqt_enable_warnings(qindaqt_window_restore_state_tests)
endif()
add_test(NAME qindaqt.hybrid-window-restore-state COMMAND qindaqt_window_restore_state_tests)

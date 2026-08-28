# SPDX-License-Identifier: GPL-3.0-or-later

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.25)
    project(QindaQtShellTaskListTests LANGUAGES CXX)

    include(CTest)
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake")
    include(QindaQtCompiler)
    find_package(ECM 6.0 REQUIRED NO_MODULE)
    list(APPEND CMAKE_MODULE_PATH "${ECM_MODULE_PATH}")
    include(KDEInstallDirs6)
    find_package(Qt6 6.11 REQUIRED COMPONENTS Core Test)
    qt_standard_project_setup(REQUIRES 6.11)

    add_subdirectory(
        "../../../src/shell/task_list"
        "${CMAKE_BINARY_DIR}/src/shell/task_list"
    )
endif()

function(qindaqt_add_task_list_test target source test_name)
    qt_add_executable(${target} ${source})
    target_link_libraries(${target} PRIVATE QindaQt::ShellTaskList Qt6::Test)
    qindaqt_enable_warnings(${target})
    add_test(NAME ${test_name} COMMAND ${target})
    set_tests_properties(${test_name} PROPERTIES LABELS "shell;task-list;pure")
endfunction()

qindaqt_add_task_list_test(
    qindaqt_task_list_values_tests
    tst_task_list_values.cpp
    qindaqt.task-list-values
)
qindaqt_add_task_list_test(
    qindaqt_task_list_source_grouping_tests
    tst_task_list_source_grouping.cpp
    qindaqt.task-list-source-grouping
)
qindaqt_add_task_list_test(
    qindaqt_task_list_source_validation_tests
    tst_task_list_source_validation.cpp
    qindaqt.task-list-source-validation
)
qindaqt_add_task_list_test(
    qindaqt_task_list_intents_tests
    tst_task_list_intents.cpp
    qindaqt.task-list-intents
)
qindaqt_add_task_list_test(
    qindaqt_task_list_scope_filter_tests
    tst_task_list_scope_filter.cpp
    qindaqt.task-list-scope-filter
)
qindaqt_add_task_list_test(
    qindaqt_task_list_presentation_tests
    tst_task_list_presentation.cpp
    qindaqt.task-list-presentation
)

add_test(
    NAME qindaqt.task-list-boundary
    COMMAND
        "${CMAKE_COMMAND}"
        "-DSOURCE_ROOT=${CMAKE_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/check_task_list_boundary.cmake"
)
set_tests_properties(qindaqt.task-list-boundary PROPERTIES
    LABELS "shell;task-list;pure;policy"
)

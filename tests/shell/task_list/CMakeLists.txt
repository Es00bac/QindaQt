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
    find_package(Qt6 6.11 REQUIRED COMPONENTS Core DBus Test)
    qt_standard_project_setup(REQUIRES 6.11)

    add_subdirectory(
        "../../../src/shell_visibility_protocol"
        "${CMAKE_BINARY_DIR}/src/shell_visibility_protocol"
    )
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

function(qindaqt_add_task_list_transport_test target source test_name)
    qt_add_executable(${target} ${source})
    target_link_libraries(
        ${target}
        PRIVATE
            QindaQt::ShellTaskList
            QindaQt::ShellTaskListProducer
            QindaQt::ShellTaskListOperations
            Qt6::DBus
            Qt6::Test
    )
    qindaqt_enable_warnings(${target})
    add_test(NAME ${test_name} COMMAND ${target})
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

qindaqt_add_task_list_transport_test(
    qindaqt_task_list_wire_tests
    tst_task_list_wire.cpp
    qindaqt.task-list-wire
)
set_tests_properties(
    qindaqt.task-list-wire PROPERTIES LABELS "shell;task-list;pure"
)
qindaqt_add_task_list_transport_test(
    qindaqt_task_list_fact_joiner_tests
    tst_task_list_fact_joiner.cpp
    qindaqt.task-list-fact-joiner
)
set_tests_properties(
    qindaqt.task-list-fact-joiner PROPERTIES LABELS "shell;task-list;pure"
)
qindaqt_add_task_list_transport_test(
    qindaqt_task_list_facts_producer_tests
    tst_task_list_facts_producer.cpp
    qindaqt.task-list-facts-producer
)
set_tests_properties(
    qindaqt.task-list-facts-producer PROPERTIES LABELS "shell;task-list"
)
qindaqt_add_task_list_transport_test(
    qindaqt_task_list_operation_adapter_tests
    tst_task_list_operation_adapter.cpp
    qindaqt.task-list-operation-adapter
)
set_tests_properties(
    qindaqt.task-list-operation-adapter PROPERTIES LABELS "shell;task-list"
)
qindaqt_add_task_list_transport_test(
    qindaqt_task_list_qt_transports_tests
    tst_task_list_qt_transports.cpp
    qindaqt.task-list-qt-transports
)
set_tests_properties(
    qindaqt.task-list-qt-transports PROPERTIES LABELS "shell;task-list;dbus"
)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(task_list_boundary_source_root "${CMAKE_CURRENT_SOURCE_DIR}/../../..")
else()
    set(task_list_boundary_source_root "${CMAKE_SOURCE_DIR}")
endif()
add_test(
    NAME qindaqt.task-list-boundary
    COMMAND
        "${CMAKE_COMMAND}"
        "-DSOURCE_ROOT=${task_list_boundary_source_root}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/check_task_list_boundary.cmake"
)
set_tests_properties(qindaqt.task-list-boundary PROPERTIES
    LABELS "shell;task-list;pure;policy"
)

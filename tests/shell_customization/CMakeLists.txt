# SPDX-License-Identifier: GPL-3.0-or-later

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.25)
    project(QindaQtShellCustomizationTests LANGUAGES CXX)

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
    add_subdirectory("../../src/applets" "${CMAKE_BINARY_DIR}/src/applets")
    add_subdirectory(
        "../../src/shell_customization"
        "${CMAKE_BINARY_DIR}/src/shell_customization"
    )
endif()

function(qindaqt_add_shell_customization_test target source test_name)
    qt_add_executable(${target} ${source})
    target_link_libraries(
        ${target}
        PRIVATE QindaQt::ShellCustomization Qt6::Test
    )
    qindaqt_enable_warnings(${target})
    add_test(NAME ${test_name} COMMAND ${target})
endfunction()

qindaqt_add_shell_customization_test(
    qindaqt_panel_editing_tests
    tst_panel_editing.cpp
    qindaqt.shell-customization-panels
)
qindaqt_add_shell_customization_test(
    qindaqt_applet_editing_tests
    tst_applet_editing.cpp
    qindaqt.shell-customization-applets
)
qindaqt_add_shell_customization_test(
    qindaqt_preview_history_tests
    tst_preview_history.cpp
    qindaqt.shell-customization-preview-history
)
qindaqt_add_shell_customization_test(
    qindaqt_coordinator_lease_tests
    tst_coordinator_lease.cpp
    qindaqt.shell-customization-coordinator-lease
)
qindaqt_add_shell_customization_test(
    qindaqt_editor_query_tests
    tst_editor_queries.cpp
    qindaqt.shell-customization-editor-queries
)

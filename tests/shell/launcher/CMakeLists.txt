# SPDX-License-Identifier: GPL-3.0-or-later

# Launcher L0 hostile-fixture tests. The block below lets this directory
# configure and run on its own for a focused lane build; the manager lane can
# also wire it from tests/CMakeLists.txt once the module integrates.

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.25)
    project(QindaQtShellLauncherTests LANGUAGES CXX)

    include(CTest)
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake")
    include(QindaQtCompiler)
    find_package(ECM 6.0 REQUIRED NO_MODULE)
    list(APPEND CMAKE_MODULE_PATH "${ECM_MODULE_PATH}")
    include(KDEInstallDirs6)
    find_package(Qt6 6.11 REQUIRED COMPONENTS Core Test)
    qt_standard_project_setup(REQUIRES 6.11)

    add_subdirectory(
        "../../../src/shell/launcher"
        "${CMAKE_BINARY_DIR}/src/shell/launcher"
    )
endif()

function(qindaqt_add_shell_launcher_test target source test_name)
    qt_add_executable(${target} ${source})
    target_link_libraries(${target} PRIVATE QindaQt::ShellLauncher Qt6::Test)
    qindaqt_enable_warnings(${target})
    add_test(NAME ${test_name} COMMAND ${target})
endfunction()

qindaqt_add_shell_launcher_test(
    qindaqt_desktop_entry_parser_tests
    tst_desktop_entry_parser.cpp
    qindaqt.launcher-desktop-entry-parser
)
qindaqt_add_shell_launcher_test(
    qindaqt_application_catalog_tests
    tst_application_catalog.cpp
    qindaqt.launcher-application-catalog
)
qindaqt_add_shell_launcher_test(
    qindaqt_launcher_category_model_tests
    tst_launcher_category_model.cpp
    qindaqt.launcher-category-model
)
qindaqt_add_shell_launcher_test(
    qindaqt_launcher_search_ranker_tests
    tst_launcher_search_ranker.cpp
    qindaqt.launcher-search-ranker
)
qindaqt_add_shell_launcher_test(
    qindaqt_launcher_pinned_recent_tests
    tst_launcher_pinned_recent.cpp
    qindaqt.launcher-pinned-recent
)
qindaqt_add_shell_launcher_test(
    qindaqt_launcher_presentation_tests
    tst_launcher_presentation.cpp
    qindaqt.launcher-presentation
)

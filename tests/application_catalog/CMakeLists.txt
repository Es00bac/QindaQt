# SPDX-License-Identifier: LGPL-3.0-or-later

# Short public-catalog development gate. It deliberately omits the shell
# runtime, presentation modules and production process/transport adapters.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.25)
    project(QindaQtApplicationCatalogTests LANGUAGES CXX)
    include(CTest)
    include(GNUInstallDirs)
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake")
    include(QindaQtCompiler)
    find_package(Qt6 6.11 REQUIRED COMPONENTS Core Test)
    qt_standard_project_setup(REQUIRES 6.11)
    set(QINDAQT_LAUNCHER_PURE_ONLY ON)
    add_subdirectory("../../src/shell/launcher" "${CMAKE_BINARY_DIR}/src/shell/launcher")
    add_subdirectory("../../src/application_catalog" "${CMAKE_BINARY_DIR}/src/application_catalog")

    foreach(behavior IN ITEMS application_catalog desktop_entry_parser launcher_category_model
                              launcher_search_ranker launcher_pinned_recent launcher_presentation)
        qt_add_executable(qindaqt_catalog_${behavior}_regression "../shell/launcher/tst_${behavior}.cpp")
        target_link_libraries(qindaqt_catalog_${behavior}_regression PRIVATE QindaQt::ShellLauncher Qt6::Test)
        qindaqt_enable_warnings(qindaqt_catalog_${behavior}_regression)
        add_test(NAME application-catalog.regression-${behavior} COMMAND qindaqt_catalog_${behavior}_regression)
    endforeach()
endif()

qt_add_executable(qindaqt_application_catalog_category_tree_tests tst_category_tree.cpp)
target_link_libraries(
    qindaqt_application_catalog_category_tree_tests
    PRIVATE QindaQt::ApplicationCatalog Qt6::Test
)
qindaqt_enable_warnings(qindaqt_application_catalog_category_tree_tests)
add_test(
    NAME application-catalog.category-tree
    COMMAND qindaqt_application_catalog_category_tree_tests
)

qt_add_executable(qindaqt_application_directory_scan_tests tst_application_directory_scan.cpp)
target_link_libraries(
    qindaqt_application_directory_scan_tests
    PRIVATE QindaQt::ApplicationCatalog Qt6::Test
)
qindaqt_enable_warnings(qindaqt_application_directory_scan_tests)
add_test(
    NAME application-catalog.directory-scan
    COMMAND qindaqt_application_directory_scan_tests
)

qt_add_executable(qindaqt_application_launch_support_tests tst_launch_support.cpp)
target_link_libraries(
    qindaqt_application_launch_support_tests
    PRIVATE QindaQt::ApplicationCatalog Qt6::Test
)
qindaqt_enable_warnings(qindaqt_application_launch_support_tests)
add_test(
    NAME application-catalog.launch-support
    COMMAND qindaqt_application_launch_support_tests
)

foreach(behavior IN ITEMS catalog launch scan)
    qt_add_executable(qindaqt_managed_application_${behavior}_tests tst_managed_application_${behavior}.cpp)
    target_link_libraries(qindaqt_managed_application_${behavior}_tests PRIVATE QindaQt::ApplicationCatalog Qt6::Test)
    qindaqt_enable_warnings(qindaqt_managed_application_${behavior}_tests)
    add_test(NAME application-catalog.managed-${behavior} COMMAND qindaqt_managed_application_${behavior}_tests)
endforeach()

# SPDX-License-Identifier: LGPL-3.0-or-later

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

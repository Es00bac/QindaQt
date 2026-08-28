# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_executable(qindaqt_global_menu_ownership_tests tst_menu_ownership.cpp)
target_link_libraries(
    qindaqt_global_menu_ownership_tests
    PRIVATE QindaQt::GlobalMenuOwnership QindaQt::GlobalMenuProtocol Qt6::Test
)
qindaqt_enable_warnings(qindaqt_global_menu_ownership_tests)
add_test(
    NAME qindaqt.global-menu-ownership
    COMMAND qindaqt_global_menu_ownership_tests
)

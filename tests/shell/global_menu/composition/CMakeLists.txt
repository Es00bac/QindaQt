# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_executable(qindaqt_global_menu_composition_tests tst_menu_composition.cpp)
target_link_libraries(
    qindaqt_global_menu_composition_tests
    PRIVATE
        QindaQt::GlobalMenuOwnership
        QindaQt::GlobalMenuExporter
        QindaQt::GlobalMenuProtocol
        Qt6::Test
)
qindaqt_enable_warnings(qindaqt_global_menu_composition_tests)
add_test(
    NAME qindaqt.global-menu-composition
    COMMAND qindaqt_global_menu_composition_tests
)

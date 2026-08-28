# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_executable(qindaqt_global_menu_exporter_tests tst_menu_exporter.cpp)
target_link_libraries(
    qindaqt_global_menu_exporter_tests
    PRIVATE QindaQt::GlobalMenuExporter QindaQt::GlobalMenuProtocol Qt6::Test
)
qindaqt_enable_warnings(qindaqt_global_menu_exporter_tests)
add_test(
    NAME qindaqt.global-menu-exporter
    COMMAND qindaqt_global_menu_exporter_tests
)

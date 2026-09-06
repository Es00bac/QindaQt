# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_executable(qindaqt_global_menu_applet_access_tests tst_globalmenuappletaccess.cpp)
target_link_libraries(
    qindaqt_global_menu_applet_access_tests
    PRIVATE QindaQt::GlobalMenuApplet QindaQt::GlobalMenuProtocol Qt6::Test
)
qindaqt_enable_warnings(qindaqt_global_menu_applet_access_tests)
add_test(
    NAME qindaqt.global-menu-applet-access
    COMMAND qindaqt_global_menu_applet_access_tests
)

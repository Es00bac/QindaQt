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

# ADR-0260: the desktop channel and provider confirmations, split from the
# application-channel rows above by behaviour.
qt_add_executable(
    qindaqt_global_menu_applet_access_desktop_tests
    tst_globalmenuappletaccess_desktop.cpp
)
target_link_libraries(
    qindaqt_global_menu_applet_access_desktop_tests
    PRIVATE QindaQt::GlobalMenuApplet QindaQt::GlobalMenuProtocol Qt6::Test
)
qindaqt_enable_warnings(qindaqt_global_menu_applet_access_desktop_tests)
add_test(
    NAME qindaqt.global-menu-applet-access-desktop
    COMMAND qindaqt_global_menu_applet_access_desktop_tests
)
set_tests_properties(
    qindaqt.global-menu-applet-access-desktop
    PROPERTIES LABELS "shell;global-menu;desktop-menu"
)

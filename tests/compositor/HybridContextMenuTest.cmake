# SPDX-License-Identifier: GPL-3.0-or-later

# AGENT-NOTE: Keep this QWidget-focused target out of the compositor registry;
# that file is already the coordination point for every Hybrid test family.
qt_add_executable(
    qindaqt_kwin_group_context_menu_tests
    tst_kwingroupcontextmenu.cpp
)
target_link_libraries(
    qindaqt_kwin_group_context_menu_tests
    PRIVATE qindaqt_group_context_menu Qt6::Widgets Qt6::Test
)
add_test(
    NAME compositor.kwin-group-context-menu
    COMMAND qindaqt_kwin_group_context_menu_tests
)
set_tests_properties(
    compositor.kwin-group-context-menu
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

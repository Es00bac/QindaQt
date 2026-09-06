# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_executable(qindaqt_decoration_plugin_tests tst_decorationplugin.cpp)
target_link_libraries(qindaqt_decoration_plugin_tests PRIVATE Qt6::Core Qt6::Test)
target_compile_definitions(
    qindaqt_decoration_plugin_tests
    PRIVATE QINDAQT_DECORATION_PLUGIN="$<TARGET_FILE:qindaqt_decoration>"
)
add_dependencies(qindaqt_decoration_plugin_tests qindaqt_decoration)
qindaqt_enable_warnings(qindaqt_decoration_plugin_tests)
add_test(NAME hybrid.decoration-plugin-load COMMAND qindaqt_decoration_plugin_tests)

qt_add_executable(qindaqt_decoration_visual_tests tst_decorationvisuals.cpp)
target_link_libraries(
    qindaqt_decoration_visual_tests
    PRIVATE
        Qt6::Gui
        Qt6::Test
        qindaqt_decoration_visuals
)
qindaqt_enable_warnings(qindaqt_decoration_visual_tests)
add_test(NAME hybrid.decoration-visuals COMMAND qindaqt_decoration_visual_tests)

qt_add_executable(qindaqt_window_context_menu_tests tst_windowcontextmenu.cpp)
target_link_libraries(
    qindaqt_window_context_menu_tests
    PRIVATE
        Qt6::Test
        qindaqt_decoration_context_menu
)
qindaqt_enable_warnings(qindaqt_window_context_menu_tests)
add_test(NAME hybrid.window-context-menu COMMAND qindaqt_window_context_menu_tests)
set_tests_properties(hybrid.window-context-menu PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

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

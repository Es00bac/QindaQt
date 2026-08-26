# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_executable(qindaqt_theme_tests tst_themes.cpp)
target_link_libraries(qindaqt_theme_tests PRIVATE qindaqt_themes Qt6::Test)
target_compile_definitions(qindaqt_theme_tests PRIVATE QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
qindaqt_enable_warnings(qindaqt_theme_tests)
add_test(NAME qindaqt.theme-formats COMMAND qindaqt_theme_tests)

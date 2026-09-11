# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_executable(qindaqt_decoration_painter_tests tst_decoration_painter.cpp)
target_link_libraries(
    qindaqt_decoration_painter_tests
    PRIVATE QindaQt::DecorationPainter Qt6::Gui Qt6::Test
)
qindaqt_enable_warnings(qindaqt_decoration_painter_tests)
add_test(NAME qindaqt.decoration-painter COMMAND qindaqt_decoration_painter_tests)
set_tests_properties(
    qindaqt.decoration-painter
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_executable(qindaqt_decoration_painter_tests tst_decoration_painter.cpp)
target_link_libraries(
    qindaqt_decoration_painter_tests
    PRIVATE QindaQt::DecorationPainter Qt6::Gui Qt6::Test
)
target_compile_definitions(
    qindaqt_decoration_painter_tests
    PRIVATE QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)
qindaqt_enable_warnings(qindaqt_decoration_painter_tests)
add_test(NAME qindaqt.decoration-painter COMMAND qindaqt_decoration_painter_tests)
set_tests_properties(
    qindaqt.decoration-painter
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

qt_add_executable(qindaqt_member_handle_layout_tests tst_member_handle_layout.cpp)
target_link_libraries(
    qindaqt_member_handle_layout_tests
    PRIVATE QindaQt::DecorationPainter Qt6::Gui Qt6::Test
)
qindaqt_enable_warnings(qindaqt_member_handle_layout_tests)
add_test(NAME qindaqt.member-handle-layout COMMAND qindaqt_member_handle_layout_tests)
set_tests_properties(
    qindaqt.member-handle-layout
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

qt_add_executable(qindaqt_decoration_documents_tests tst_decoration_documents.cpp)
target_link_libraries(
    qindaqt_decoration_documents_tests
    PRIVATE QindaQt::DecorationPainter qindaqt_themes Qt6::Gui Qt6::Test
)
target_compile_definitions(
    qindaqt_decoration_documents_tests
    PRIVATE QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)
qindaqt_enable_warnings(qindaqt_decoration_documents_tests)
add_test(NAME qindaqt.decoration-documents COMMAND qindaqt_decoration_documents_tests)
set_tests_properties(
    qindaqt.decoration-documents
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

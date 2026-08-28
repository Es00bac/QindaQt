# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_executable(qindaqt_clipboard_media_tests tst_clipboard_media.cpp)
target_link_libraries(
    qindaqt_clipboard_media_tests
    PRIVATE QindaQt::ClipboardModel Qt6::Test
)
qindaqt_enable_warnings(qindaqt_clipboard_media_tests)
add_test(NAME qindaqt.clipboard-model-media COMMAND qindaqt_clipboard_media_tests)
set_tests_properties(qindaqt.clipboard-model-media PROPERTIES LABELS "service;clipboard")

qt_add_executable(qindaqt_clipboard_history_tests tst_clipboard_history.cpp)
target_link_libraries(
    qindaqt_clipboard_history_tests
    PRIVATE QindaQt::ClipboardModel Qt6::Test
)
qindaqt_enable_warnings(qindaqt_clipboard_history_tests)
add_test(NAME qindaqt.clipboard-model-history COMMAND qindaqt_clipboard_history_tests)
set_tests_properties(qindaqt.clipboard-model-history PROPERTIES LABELS "service;clipboard")

qt_add_executable(
    qindaqt_clipboard_history_lineage_tests tst_clipboard_history_lineage.cpp
)
target_link_libraries(
    qindaqt_clipboard_history_lineage_tests
    PRIVATE QindaQt::ClipboardModel Qt6::Test
)
qindaqt_enable_warnings(qindaqt_clipboard_history_lineage_tests)
add_test(
    NAME qindaqt.clipboard-model-history-lineage
    COMMAND qindaqt_clipboard_history_lineage_tests
)
set_tests_properties(
    qindaqt.clipboard-model-history-lineage PROPERTIES LABELS "service;clipboard"
)

qt_add_executable(qindaqt_clipboard_codec_tests tst_clipboard_codec.cpp)
target_link_libraries(
    qindaqt_clipboard_codec_tests
    PRIVATE QindaQt::ClipboardModel Qt6::Test
)
qindaqt_enable_warnings(qindaqt_clipboard_codec_tests)
add_test(NAME qindaqt.clipboard-model-codec COMMAND qindaqt_clipboard_codec_tests)
set_tests_properties(qindaqt.clipboard-model-codec PROPERTIES LABELS "service;clipboard")

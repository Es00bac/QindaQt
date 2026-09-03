# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_executable(qindaqt_clipboard_protocol_tests tst_clipboard_protocol.cpp)
target_link_libraries(qindaqt_clipboard_protocol_tests PRIVATE QindaQt::ClipboardProtocol Qt6::DBus Qt6::Test)
qindaqt_enable_warnings(qindaqt_clipboard_protocol_tests)
add_test(NAME qindaqt.clipboard-protocol COMMAND qindaqt_clipboard_protocol_tests)
set_tests_properties(qindaqt.clipboard-protocol PROPERTIES LABELS "service;clipboard;protocol" TIMEOUT 20)

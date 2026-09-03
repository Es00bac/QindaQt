# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_executable(qindaqt_clipboard_client_tests tst_clipboard_client.cpp support/fake_clipboard_transport.h)
target_link_libraries(qindaqt_clipboard_client_tests PRIVATE QindaQt::ClipboardClient Qt6::Test)
qindaqt_enable_warnings(qindaqt_clipboard_client_tests)
add_test(NAME qindaqt.clipboard-client COMMAND qindaqt_clipboard_client_tests)
set_tests_properties(qindaqt.clipboard-client PROPERTIES LABELS "service;clipboard;client" TIMEOUT 20)

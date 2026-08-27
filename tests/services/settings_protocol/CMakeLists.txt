# SPDX-License-Identifier: GPL-3.0-or-later
qt_add_executable(qindaqt_settings_protocol_tests tst_settings_protocol.cpp)
target_link_libraries(qindaqt_settings_protocol_tests PRIVATE QindaQt::SettingsProtocol Qt6::Test)
qindaqt_enable_warnings(qindaqt_settings_protocol_tests)
add_test(NAME qindaqt.settings-protocol COMMAND qindaqt_settings_protocol_tests)

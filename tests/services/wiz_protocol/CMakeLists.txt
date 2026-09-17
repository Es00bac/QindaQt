# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_executable(qindaqt_wiz_protocol_tests tst_wiz_protocol.cpp)
target_link_libraries(
    qindaqt_wiz_protocol_tests PRIVATE QindaQt::WizProtocol Qt6::Test
)
qindaqt_enable_warnings(qindaqt_wiz_protocol_tests)
add_test(NAME qindaqt.wiz-protocol COMMAND qindaqt_wiz_protocol_tests)
set_tests_properties(
    qindaqt.wiz-protocol PROPERTIES LABELS "service;smart-lights;pure"
)

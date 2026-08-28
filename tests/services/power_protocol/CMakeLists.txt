# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_executable(qindaqt_power_protocol_values_tests tst_power_protocol_values.cpp)
target_link_libraries(
    qindaqt_power_protocol_values_tests
    PRIVATE QindaQt::PowerProtocol Qt6::DBus Qt6::Test
)
qindaqt_enable_warnings(qindaqt_power_protocol_values_tests)
add_test(NAME qindaqt.power-protocol-values COMMAND qindaqt_power_protocol_values_tests)
set_tests_properties(qindaqt.power-protocol-values PROPERTIES LABELS "service;power;pure")

qt_add_executable(qindaqt_power_protocol_codec_tests tst_power_protocol_codec.cpp)
target_link_libraries(
    qindaqt_power_protocol_codec_tests
    PRIVATE QindaQt::PowerProtocol Qt6::Test
)
qindaqt_enable_warnings(qindaqt_power_protocol_codec_tests)
add_test(NAME qindaqt.power-protocol-codec COMMAND qindaqt_power_protocol_codec_tests)
set_tests_properties(qindaqt.power-protocol-codec PROPERTIES LABELS "service;power;pure")

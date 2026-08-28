# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_executable(qindaqt_display_protocol_value_tests tst_display_protocol_values.cpp)
target_link_libraries(
    qindaqt_display_protocol_value_tests
    PRIVATE QindaQt::DisplayProtocol Qt6::DBus Qt6::Test
)
qindaqt_enable_warnings(qindaqt_display_protocol_value_tests)
add_test(NAME qindaqt.display-protocol-values COMMAND qindaqt_display_protocol_value_tests)
set_tests_properties(qindaqt.display-protocol-values PROPERTIES LABELS "service;display")

qt_add_executable(qindaqt_display_protocol_codec_tests tst_display_protocol_codec.cpp)
target_link_libraries(
    qindaqt_display_protocol_codec_tests
    PRIVATE QindaQt::DisplayProtocol Qt6::DBus Qt6::Test
)
qindaqt_enable_warnings(qindaqt_display_protocol_codec_tests)
add_test(NAME qindaqt.display-protocol-codec COMMAND qindaqt_display_protocol_codec_tests)
set_tests_properties(qindaqt.display-protocol-codec PROPERTIES LABELS "service;display")

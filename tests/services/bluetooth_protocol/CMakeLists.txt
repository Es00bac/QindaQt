# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_executable(qindaqt_bluetooth_protocol_tests tst_bluetooth_protocol.cpp)
target_link_libraries(
    qindaqt_bluetooth_protocol_tests
    PRIVATE QindaQt::BluetoothProtocol Qt6::DBus Qt6::Test
)
qindaqt_enable_warnings(qindaqt_bluetooth_protocol_tests)
add_test(NAME qindaqt.bluetooth-protocol COMMAND qindaqt_bluetooth_protocol_tests)
set_tests_properties(
    qindaqt.bluetooth-protocol PROPERTIES LABELS "service;bluetooth"
)

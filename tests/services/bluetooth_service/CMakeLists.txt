# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_executable(
    qindaqt_bluetooth_service_tests
    tst_bluetooth_service.cpp
    support/fake_adapter_backend.h
)
target_link_libraries(
    qindaqt_bluetooth_service_tests
    PRIVATE QindaQt::BluetoothService QindaQt::BluetoothModel Qt6::DBus Qt6::Test
)
qindaqt_enable_warnings(qindaqt_bluetooth_service_tests)
add_test(NAME qindaqt.bluetooth-service COMMAND qindaqt_bluetooth_service_tests)
set_tests_properties(
    qindaqt.bluetooth-service PROPERTIES LABELS "service;bluetooth" TIMEOUT 20
)

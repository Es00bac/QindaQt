# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_executable(
    qindaqt_bluetooth_model_tests
    tst_bluetooth_model.cpp
    support/fake_adapter_backend.h
)
target_link_libraries(
    qindaqt_bluetooth_model_tests
    PRIVATE QindaQt::BluetoothModel Qt6::DBus Qt6::Test
)
qindaqt_enable_warnings(qindaqt_bluetooth_model_tests)
add_test(NAME qindaqt.bluetooth-model COMMAND qindaqt_bluetooth_model_tests)
set_tests_properties(qindaqt.bluetooth-model PROPERTIES LABELS "service;bluetooth")

qt_add_executable(qindaqt_bluetooth_deterministic_backend_tests
    tst_deterministic_adapter_backend.cpp
)
target_include_directories(
    qindaqt_bluetooth_deterministic_backend_tests
    PRIVATE ${CMAKE_SOURCE_DIR}/src/services/bluetooth_model/src
)
target_link_libraries(
    qindaqt_bluetooth_deterministic_backend_tests
    PRIVATE QindaQt::BluetoothModel Qt6::DBus Qt6::Test
)
qindaqt_enable_warnings(qindaqt_bluetooth_deterministic_backend_tests)
add_test(
    NAME qindaqt.bluetooth-deterministic-backend
    COMMAND qindaqt_bluetooth_deterministic_backend_tests
)
set_tests_properties(
    qindaqt.bluetooth-deterministic-backend PROPERTIES LABELS "service;bluetooth"
)

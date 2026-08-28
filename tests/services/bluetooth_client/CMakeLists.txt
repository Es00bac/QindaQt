# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_executable(
    qindaqt_bluetooth_client_tests
    tst_bluetooth_client.cpp
    support/fake_bluetooth_transport.h
)
target_link_libraries(
    qindaqt_bluetooth_client_tests
    PRIVATE QindaQt::BluetoothClient Qt6::DBus Qt6::Test
)
qindaqt_enable_warnings(qindaqt_bluetooth_client_tests)
add_test(NAME qindaqt.bluetooth-client COMMAND qindaqt_bluetooth_client_tests)
set_tests_properties(
    qindaqt.bluetooth-client PROPERTIES TIMEOUT 20 LABELS "service;bluetooth"
)

qt_add_executable(
    qindaqt_bluetooth_qt_transport_tests
    tst_qt_bluetooth_transport.cpp
)
target_include_directories(
    qindaqt_bluetooth_qt_transport_tests
    PRIVATE ${CMAKE_SOURCE_DIR}/src/services/bluetooth_model/src
)
target_link_libraries(
    qindaqt_bluetooth_qt_transport_tests
    PRIVATE
        QindaQt::BluetoothClient
        QindaQt::BluetoothService
        QindaQt::BluetoothModel
        Qt6::Core
        Qt6::DBus
        Qt6::Test
)
qindaqt_enable_warnings(qindaqt_bluetooth_qt_transport_tests)
add_test(NAME qindaqt.bluetooth-qt-transport COMMAND qindaqt_bluetooth_qt_transport_tests)
set_tests_properties(
    qindaqt.bluetooth-qt-transport
    PROPERTIES TIMEOUT 20 LABELS "service;bluetooth;dbus"
)

find_program(QINDAQT_DBUS_DAEMON_EXECUTABLE NAMES dbus-daemon REQUIRED)
qt_add_executable(qindaqt_bluetooth_activation_tests tst_bluetooth_activation.cpp)
target_link_libraries(
    qindaqt_bluetooth_activation_tests
    PRIVATE QindaQt::BluetoothClient Qt6::DBus Qt6::Test
)
target_compile_definitions(
    qindaqt_bluetooth_activation_tests
    PRIVATE
        QINDAQT_BLUETOOTH_SERVICE_EXECUTABLE="$<TARGET_FILE:qindaqt-bluetooth-service>"
        QINDAQT_DBUS_DAEMON_EXECUTABLE="${QINDAQT_DBUS_DAEMON_EXECUTABLE}"
)
qindaqt_enable_warnings(qindaqt_bluetooth_activation_tests)
add_dependencies(qindaqt_bluetooth_activation_tests qindaqt-bluetooth-service)
add_test(NAME qindaqt.bluetooth-activation COMMAND qindaqt_bluetooth_activation_tests)
set_tests_properties(
    qindaqt.bluetooth-activation
    PROPERTIES LABELS "service;bluetooth;dbus;activation" TIMEOUT 20 RUN_SERIAL TRUE
)

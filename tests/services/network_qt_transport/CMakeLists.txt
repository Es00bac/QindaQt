# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_executable(
    qindaqt_network_qt_transport_tests
    tst_qt_network_transport.cpp
    ../network_service/support/fake_network_backend.h
)
target_link_libraries(
    qindaqt_network_qt_transport_tests
    PRIVATE
        QindaQt::NetworkQtTransport
        QindaQt::NetworkService
        QindaQt::NetworkProtocol
        Qt6::DBus
        Qt6::Test
)
qindaqt_enable_warnings(qindaqt_network_qt_transport_tests)
add_test(
    NAME qindaqt.network-qt-transport
    COMMAND qindaqt_network_qt_transport_tests
)
set_tests_properties(
    qindaqt.network-qt-transport
    PROPERTIES LABELS "service;network;dbus" TIMEOUT 30
)

find_program(QINDAQT_DBUS_DAEMON_EXECUTABLE NAMES dbus-daemon REQUIRED)
qt_add_executable(qindaqt_network_activation_tests tst_network_activation.cpp)
target_link_libraries(
    qindaqt_network_activation_tests
    PRIVATE
        QindaQt::NetworkQtTransport
        QindaQt::NetworkClient
        Qt6::DBus
        Qt6::Test
)
target_compile_definitions(
    qindaqt_network_activation_tests
    PRIVATE
        QINDAQT_NETWORK_SERVICE_EXECUTABLE="$<TARGET_FILE:qindaqt-network-service>"
        QINDAQT_DBUS_DAEMON_EXECUTABLE="${QINDAQT_DBUS_DAEMON_EXECUTABLE}"
)
qindaqt_enable_warnings(qindaqt_network_activation_tests)
add_dependencies(qindaqt_network_activation_tests qindaqt-network-service)
add_test(NAME qindaqt.network-activation COMMAND qindaqt_network_activation_tests)
set_tests_properties(
    qindaqt.network-activation
    PROPERTIES LABELS "service;network;dbus;activation" TIMEOUT 60 RUN_SERIAL TRUE
)

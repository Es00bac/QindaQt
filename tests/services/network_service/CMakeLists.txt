# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_executable(
    qindaqt_network_service_tests
    tst_network_service.cpp
    support/fake_network_backend.h
)
target_link_libraries(
    qindaqt_network_service_tests
    PRIVATE QindaQt::NetworkService Qt6::Test
)
qindaqt_enable_warnings(qindaqt_network_service_tests)
add_test(NAME qindaqt.network-service COMMAND qindaqt_network_service_tests)
set_tests_properties(
    qindaqt.network-service
    PROPERTIES LABELS "service;network" TIMEOUT 20
)

qt_add_executable(
    qindaqt_network_residency_tests
    tst_network_residency.cpp
    support/fake_network_backend.h
)
target_link_libraries(
    qindaqt_network_residency_tests
    PRIVATE
        QindaQt::NetworkService
        QindaQt::NetworkQtTransport
        QindaQt::NetworkClient
        Qt6::DBus
        Qt6::Test
)
qindaqt_enable_warnings(qindaqt_network_residency_tests)
add_test(NAME qindaqt.network-residency COMMAND qindaqt_network_residency_tests)
set_tests_properties(
    qindaqt.network-residency
    PROPERTIES LABELS "service;network;dbus" TIMEOUT 30
)

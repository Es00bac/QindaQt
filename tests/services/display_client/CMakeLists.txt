# SPDX-License-Identifier: GPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Core DBus Test)
find_program(QINDAQT_DISPLAY_DBUS_DAEMON_EXECUTABLE dbus-daemon REQUIRED)

function(qindaqt_add_display_client_test target source test_name)
    qt_add_executable(
        ${target}
        ${source}
        support/display_client_test_support.h
        support/fake_display_transport.h
    )
    target_link_libraries(
        ${target}
        PRIVATE QindaQt::DisplayClient Qt6::Core Qt6::DBus Qt6::Test
    )
    target_compile_definitions(
        ${target}
        PRIVATE QINDAQT_DBUS_DAEMON_EXECUTABLE="${QINDAQT_DISPLAY_DBUS_DAEMON_EXECUTABLE}"
    )
    qindaqt_enable_warnings(${target})
    add_test(NAME ${test_name} COMMAND ${target})
    set_tests_properties(${test_name} PROPERTIES LABELS "client;display" TIMEOUT 30)
endfunction()

qindaqt_add_display_client_test(
    qindaqt_display_client_lineage_tests
    tst_display_client_lineage.cpp
    qindaqt.display-client-lineage
)
qindaqt_add_display_client_test(
    qindaqt_display_client_publication_tests
    tst_display_client_publication.cpp
    qindaqt.display-client-publication
)
qindaqt_add_display_client_test(
    qindaqt_display_client_operations_tests
    tst_display_client_operations.cpp
    qindaqt.display-client-operations
)
qindaqt_add_display_client_test(
    qindaqt_display_client_coordinator_tests
    tst_display_client_coordinator.cpp
    qindaqt.display-client-coordinator
)

qt_add_executable(
    qindaqt_display_client_private_bus_tests
    tst_display_client_private_bus.cpp
    support/display_client_test_support.h
    support/display_client_private_bus_support.h
)
target_link_libraries(
    qindaqt_display_client_private_bus_tests
    PRIVATE
        QindaQt::DisplayClient
        QindaQt::DisplayService
        QindaQt::DisplayTopology
        Qt6::Core
        Qt6::DBus
        Qt6::Test
)
target_compile_definitions(
    qindaqt_display_client_private_bus_tests
    PRIVATE QINDAQT_DBUS_DAEMON_EXECUTABLE="${QINDAQT_DISPLAY_DBUS_DAEMON_EXECUTABLE}"
)
qindaqt_enable_warnings(qindaqt_display_client_private_bus_tests)
add_test(
    NAME qindaqt.display-client-private-bus
    COMMAND qindaqt_display_client_private_bus_tests
)
set_tests_properties(
    qindaqt.display-client-private-bus
    PROPERTIES
        LABELS "client;display;private-dbus;isolated-runtime"
        TIMEOUT 30
        RUN_SERIAL TRUE
)

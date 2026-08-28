# SPDX-License-Identifier: GPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Core DBus Test)
find_program(QINDAQT_DISPLAY_DBUS_DAEMON_EXECUTABLE dbus-daemon REQUIRED)

qt_add_executable(
    qindaqt_display_inventory_adapter_tests
    tst_display_inventory_adapter.cpp
    support/display_service_test_support.h
)
target_link_libraries(
    qindaqt_display_inventory_adapter_tests
    PRIVATE QindaQt::DisplayService Qt6::Core Qt6::DBus Qt6::Test
)
qindaqt_enable_warnings(qindaqt_display_inventory_adapter_tests)
add_test(
    NAME qindaqt.display-service-inventory
    COMMAND qindaqt_display_inventory_adapter_tests
)

qt_add_executable(
    qindaqt_display_service_model_tests
    tst_display_service_model.cpp
    support/display_service_test_support.h
)
target_link_libraries(
    qindaqt_display_service_model_tests
    PRIVATE QindaQt::DisplayService Qt6::Core Qt6::DBus Qt6::Test
)
qindaqt_enable_warnings(qindaqt_display_service_model_tests)
add_test(
    NAME qindaqt.display-service-model
    COMMAND qindaqt_display_service_model_tests
)

qt_add_executable(
    qindaqt_display_service_deployment_tests
    tst_display_service_deployment.cpp
    support/display_service_test_support.h
)
target_link_libraries(
    qindaqt_display_service_deployment_tests
    PRIVATE QindaQt::DisplayService Qt6::Core Qt6::DBus Qt6::Test
)
target_compile_definitions(
    qindaqt_display_service_deployment_tests
    PRIVATE
        QINDAQT_DISPLAY_ACTIVATION_DESCRIPTOR="${CMAKE_SOURCE_DIR}/src/services/display_service/data/org.qindaqt.Display1.service.in"
        QINDAQT_DISPLAY_SYSTEMD_UNIT="${CMAKE_SOURCE_DIR}/src/services/display_service/data/qindaqt-display-service.service.in"
        QINDAQT_DISPLAY_DBUS_XML="${CMAKE_SOURCE_DIR}/src/services/display_service/data/org.qindaqt.Display1.xml"
)
qindaqt_enable_warnings(qindaqt_display_service_deployment_tests)
add_test(
    NAME qindaqt.display-service-deployment
    COMMAND qindaqt_display_service_deployment_tests
)

qt_add_executable(
    qindaqt_compositor_inventory_source_private_bus_tests
    tst_compositor_inventory_source_private_bus.cpp
    support/private_bus_test_support.h
)
target_link_libraries(
    qindaqt_compositor_inventory_source_private_bus_tests
    PRIVATE QindaQt::DisplayService Qt6::Core Qt6::DBus Qt6::Test
)
target_compile_definitions(
    qindaqt_compositor_inventory_source_private_bus_tests
    PRIVATE
        QINDAQT_DBUS_DAEMON_EXECUTABLE="${QINDAQT_DISPLAY_DBUS_DAEMON_EXECUTABLE}"
)
qindaqt_enable_warnings(qindaqt_compositor_inventory_source_private_bus_tests)
add_test(
    NAME qindaqt.display-service-inventory-private-bus
    COMMAND qindaqt_compositor_inventory_source_private_bus_tests
)

qt_add_executable(
    qindaqt_resident_display_service_private_bus_tests
    tst_resident_display_service_private_bus.cpp
    support/display_service_test_support.h
    support/private_bus_test_support.h
)
target_link_libraries(
    qindaqt_resident_display_service_private_bus_tests
    PRIVATE QindaQt::DisplayService Qt6::Core Qt6::DBus Qt6::Test
)
target_compile_definitions(
    qindaqt_resident_display_service_private_bus_tests
    PRIVATE
        QINDAQT_DBUS_DAEMON_EXECUTABLE="${QINDAQT_DISPLAY_DBUS_DAEMON_EXECUTABLE}"
)
qindaqt_enable_warnings(qindaqt_resident_display_service_private_bus_tests)
add_test(
    NAME qindaqt.display-service-resident-private-bus
    COMMAND qindaqt_resident_display_service_private_bus_tests
)

set_tests_properties(
    qindaqt.display-service-inventory
    qindaqt.display-service-model
    qindaqt.display-service-deployment
    PROPERTIES LABELS "service;display" TIMEOUT 20
)

set_tests_properties(
    qindaqt.display-service-inventory-private-bus
    qindaqt.display-service-resident-private-bus
    PROPERTIES
        LABELS "service;display;private-dbus;isolated-runtime"
        TIMEOUT 20
        RUN_SERIAL TRUE
)

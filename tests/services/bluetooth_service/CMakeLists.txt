# SPDX-License-Identifier: GPL-3.0-or-later

find_program(QINDAQT_DBUS_DAEMON_EXECUTABLE NAMES dbus-daemon REQUIRED)

qt_add_executable(
    qindaqt_bluetooth_service_tests
    tst_bluetooth_service.cpp
    support/fake_adapter_backend.h
    support/private_bus.h
)
target_compile_definitions(
    qindaqt_bluetooth_service_tests
    PRIVATE QINDAQT_DBUS_DAEMON_EXECUTABLE="${QINDAQT_DBUS_DAEMON_EXECUTABLE}"
)
target_link_libraries(
    qindaqt_bluetooth_service_tests
    PRIVATE QindaQt::BluetoothService QindaQt::BluetoothModel Qt6::DBus Qt6::Test
)
qindaqt_enable_warnings(qindaqt_bluetooth_service_tests)
add_test(NAME qindaqt.bluetooth-service COMMAND qindaqt_bluetooth_service_tests)
set_tests_properties(
    qindaqt.bluetooth-service
    PROPERTIES LABELS "service;bluetooth;dbus" TIMEOUT 20 RUN_SERIAL TRUE
)

qt_add_executable(
    qindaqt_bluetooth_lease_owner_loss_tests
    tst_bluetooth_lease_owner_loss.cpp
    support/private_bus.h
)
target_include_directories(
    qindaqt_bluetooth_lease_owner_loss_tests
    PRIVATE ${CMAKE_SOURCE_DIR}/src/services/bluetooth_model/src
)
target_compile_definitions(
    qindaqt_bluetooth_lease_owner_loss_tests
    PRIVATE QINDAQT_DBUS_DAEMON_EXECUTABLE="${QINDAQT_DBUS_DAEMON_EXECUTABLE}"
)
target_link_libraries(
    qindaqt_bluetooth_lease_owner_loss_tests
    PRIVATE
        QindaQt::BluetoothService
        QindaQt::BluetoothModel
        QindaQt::BluetoothClient
        Qt6::DBus
        Qt6::Test
)
qindaqt_enable_warnings(qindaqt_bluetooth_lease_owner_loss_tests)
add_test(
    NAME qindaqt.bluetooth-lease-owner-loss
    COMMAND qindaqt_bluetooth_lease_owner_loss_tests
)
set_tests_properties(
    qindaqt.bluetooth-lease-owner-loss
    PROPERTIES LABELS "service;bluetooth;dbus" TIMEOUT 20 RUN_SERIAL TRUE
)

# AGENT-CONTRACT: This gate stages a clean install into a test-owned prefix
# and builds a linked consumer against the staged public payload. It prevents
# broad green suites from hiding a broken deployment surface, unconfigured
# templates, or an inaccurate installed API. Runs at CTest time; no
# activation/runtime claim is made until the serialized lane executes it.
add_test(
    NAME qindaqt.bluetooth-staged-install
    COMMAND
        "${CMAKE_COMMAND}"
        "-DQINDAQT_CMAKE=${CMAKE_COMMAND}"
        "-DQINDAQT_BUILD_DIRECTORY=${CMAKE_BINARY_DIR}"
        "-DQINDAQT_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/staged-install"
        "-DQINDAQT_CONSUMER_SOURCE_DIRECTORY=${CMAKE_CURRENT_SOURCE_DIR}/installed_consumer"
        "-DQINDAQT_CONSUMER_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/installed_consumer/installed_consumer.cpp"
        "-DQINDAQT_INSTALL_BINDIR=${CMAKE_INSTALL_BINDIR}"
        "-DQINDAQT_INSTALL_LIBDIR=${CMAKE_INSTALL_LIBDIR}"
        "-DQINDAQT_INSTALL_INCLUDEDIR=${CMAKE_INSTALL_INCLUDEDIR}"
        "-DQINDAQT_INSTALL_DATADIR=${CMAKE_INSTALL_DATADIR}"
        "-DQINDAQT_INSTALL_SYSTEMDUSERUNITDIR=${KDE_INSTALL_SYSTEMDUSERUNITDIR}"
        "-DQINDAQT_BLUETOOTH_PROTOCOL_LIBRARY_NAME=$<TARGET_FILE_NAME:qindaqt_bluetooth_protocol>"
        "-DQINDAQT_QT6_DIR=${Qt6_DIR}"
        "-DQINDAQT_GENERATOR=${CMAKE_GENERATOR}"
        "-DQINDAQT_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
        "-DQINDAQT_CONFIGURATION=$<CONFIG>"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_staged_install.cmake"
)
set_tests_properties(
    qindaqt.bluetooth-staged-install PROPERTIES LABELS "package;bluetooth"
)

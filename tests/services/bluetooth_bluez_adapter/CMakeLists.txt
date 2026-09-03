# SPDX-License-Identifier: GPL-3.0-or-later

find_program(QINDAQT_DBUS_DAEMON_EXECUTABLE NAMES dbus-daemon REQUIRED)

qt_add_executable(
    qindaqt_bluez_adapter_tests
    tst_bluez_adapter_backend.cpp
    support/bluez_harness.h
    support/bluez_compose.h
    support/fake_bluez.cpp
    support/fake_bluez_pairing.cpp
    support/fake_bluez.h
    support/private_bus.h
)
target_compile_definitions(
    qindaqt_bluez_adapter_tests
    PRIVATE QINDAQT_DBUS_DAEMON_EXECUTABLE="${QINDAQT_DBUS_DAEMON_EXECUTABLE}"
)
target_link_libraries(
    qindaqt_bluez_adapter_tests
    PRIVATE QindaQt::BluezAdapter QindaQt::BluetoothModel Qt6::DBus Qt6::Test
)
qindaqt_enable_warnings(qindaqt_bluez_adapter_tests)
add_test(NAME qindaqt.bluetooth-bluez-adapter COMMAND qindaqt_bluez_adapter_tests)
set_tests_properties(
    qindaqt.bluetooth-bluez-adapter
    PROPERTIES LABELS "service;bluetooth;bluez;dbus" TIMEOUT 60 RUN_SERIAL TRUE
)

qt_add_executable(
    qindaqt_bluez_adapter_operations_tests
    tst_bluez_adapter_operations.cpp
    support/bluez_harness.h
    support/bluez_compose.h
    support/fake_bluez.cpp
    support/fake_bluez_pairing.cpp
    support/fake_bluez.h
    support/private_bus.h
)
target_compile_definitions(
    qindaqt_bluez_adapter_operations_tests
    PRIVATE QINDAQT_DBUS_DAEMON_EXECUTABLE="${QINDAQT_DBUS_DAEMON_EXECUTABLE}"
)
target_link_libraries(
    qindaqt_bluez_adapter_operations_tests
    PRIVATE QindaQt::BluezAdapter QindaQt::BluetoothModel Qt6::DBus Qt6::Test
)
qindaqt_enable_warnings(qindaqt_bluez_adapter_operations_tests)
add_test(
    NAME qindaqt.bluetooth-bluez-operations
    COMMAND qindaqt_bluez_adapter_operations_tests
)
set_tests_properties(
    qindaqt.bluetooth-bluez-operations
    PROPERTIES LABELS "service;bluetooth;bluez;dbus" TIMEOUT 60 RUN_SERIAL TRUE
)

qt_add_executable(
    qindaqt_bluez_pairing_tests
    tst_bluez_pairing.cpp
    support/bluez_harness.h
    support/bluez_compose.h
    support/fake_bluez.cpp
    support/fake_bluez_pairing.cpp
    support/fake_bluez.h
    support/private_bus.h
)
target_compile_definitions(
    qindaqt_bluez_pairing_tests
    PRIVATE QINDAQT_DBUS_DAEMON_EXECUTABLE="${QINDAQT_DBUS_DAEMON_EXECUTABLE}"
)
target_link_libraries(
    qindaqt_bluez_pairing_tests
    PRIVATE QindaQt::BluezAdapter QindaQt::BluetoothModel Qt6::DBus Qt6::Test
)
qindaqt_enable_warnings(qindaqt_bluez_pairing_tests)
add_test(NAME qindaqt.bluetooth-bluez-pairing COMMAND qindaqt_bluez_pairing_tests)
set_tests_properties(
    qindaqt.bluetooth-bluez-pairing
    PROPERTIES LABELS "service;bluetooth;bluez;dbus;pairing" TIMEOUT 60 RUN_SERIAL TRUE
)

qt_add_executable(
    qindaqt_bluez_backend_mode_tests
    tst_bluez_backend_mode.cpp
)
target_link_libraries(
    qindaqt_bluez_backend_mode_tests
    PRIVATE QindaQt::BluezAdapter Qt6::Test
)
qindaqt_enable_warnings(qindaqt_bluez_backend_mode_tests)
add_test(
    NAME qindaqt.bluetooth-bluez-mode
    COMMAND qindaqt_bluez_backend_mode_tests
)
set_tests_properties(
    qindaqt.bluetooth-bluez-mode
    PROPERTIES LABELS "service;bluetooth;bluez" TIMEOUT 20
)

add_test(
    NAME qindaqt.bluetooth-bluez-installed-boundary
    COMMAND
        "${CMAKE_COMMAND}"
        "-DBINARY_ROOT=${CMAKE_BINARY_DIR}"
        "-DSTAGE_ROOT=${CMAKE_CURRENT_BINARY_DIR}/installed-boundary"
        "-DADAPTER_LIBRARY_NAME=$<TARGET_FILE_NAME:qindaqt_bluez_adapter>"
        "-DINSTALL_LIBDIR=${CMAKE_INSTALL_LIBDIR}"
        "-DINSTALL_INCLUDEDIR=${CMAKE_INSTALL_INCLUDEDIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/check_installed_boundary.cmake"
)
set_tests_properties(
    qindaqt.bluetooth-bluez-installed-boundary
    PROPERTIES LABELS "service;bluetooth;bluez;package" TIMEOUT 30
)

add_test(
    NAME qindaqt.bluetooth-bluez-boundary
    COMMAND
        "${CMAKE_COMMAND}"
        "-DSOURCE_ROOT=${CMAKE_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/check_boundary.cmake"
)
set_tests_properties(
    qindaqt.bluetooth-bluez-boundary
    PROPERTIES LABELS "service;bluetooth;bluez;policy"
)

add_test(
    NAME qindaqt.bluetooth-bluez-boundary-poison
    COMMAND
        "${CMAKE_COMMAND}"
        "-DBINARY_ROOT=${CMAKE_BINARY_DIR}"
        "-DPOISON_ROOT=${CMAKE_CURRENT_BINARY_DIR}/boundary-poison"
        "-DCHECK_SCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/check_boundary.cmake"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/check_boundary_negative.cmake"
)
set_tests_properties(
    qindaqt.bluetooth-bluez-boundary-poison
    PROPERTIES LABELS "service;bluetooth;bluez;policy;hostile"
)

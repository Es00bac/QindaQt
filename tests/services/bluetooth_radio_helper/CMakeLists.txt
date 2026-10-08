# SPDX-License-Identifier: GPL-3.0-or-later
qt_add_executable(qindaqt_bluetooth_radio_operation_tests tst_radio_operation.cpp)
target_link_libraries(qindaqt_bluetooth_radio_operation_tests PRIVATE
    qindaqt_bluetooth_radio_authority Qt6::Test)
qindaqt_enable_warnings(qindaqt_bluetooth_radio_operation_tests)
add_test(NAME qindaqt.bluetooth-radio-operation COMMAND qindaqt_bluetooth_radio_operation_tests)
set_tests_properties(qindaqt.bluetooth-radio-operation PROPERTIES
    LABELS "service;bluetooth;radio;authority" TIMEOUT 20)

find_program(QINDAQT_DBUS_DAEMON_EXECUTABLE NAMES dbus-daemon REQUIRED)
qt_add_executable(qindaqt_bluetooth_radio_intent_tests tst_radio_intent.cpp)
target_compile_definitions(qindaqt_bluetooth_radio_intent_tests PRIVATE
    QINDAQT_DBUS_DAEMON_EXECUTABLE="${QINDAQT_DBUS_DAEMON_EXECUTABLE}")
target_link_libraries(qindaqt_bluetooth_radio_intent_tests PRIVATE QindaQt::BluetoothRadioClient Qt6::Test)
qindaqt_enable_warnings(qindaqt_bluetooth_radio_intent_tests)
add_test(NAME qindaqt.bluetooth-radio-intent COMMAND qindaqt_bluetooth_radio_intent_tests)
set_tests_properties(qindaqt.bluetooth-radio-intent PROPERTIES
    LABELS "service;bluetooth;radio;authority;dbus" TIMEOUT 20 RUN_SERIAL TRUE)

qt_add_executable(qindaqt_bluetooth_radio_authority_tests tst_radio_authority.cpp)
target_compile_definitions(qindaqt_bluetooth_radio_authority_tests PRIVATE
    QINDAQT_DBUS_DAEMON_EXECUTABLE="${QINDAQT_DBUS_DAEMON_EXECUTABLE}")
target_link_libraries(qindaqt_bluetooth_radio_authority_tests PRIVATE
    qindaqt_bluetooth_radio_authority Qt6::Test)
qindaqt_enable_warnings(qindaqt_bluetooth_radio_authority_tests)
add_test(NAME qindaqt.bluetooth-radio-authority COMMAND qindaqt_bluetooth_radio_authority_tests)
set_tests_properties(qindaqt.bluetooth-radio-authority PROPERTIES
    LABELS "service;bluetooth;radio;authority;dbus" TIMEOUT 30 RUN_SERIAL TRUE)

add_test(NAME qindaqt.bluetooth-radio-boundary COMMAND "${CMAKE_COMMAND}"
    "-DSOURCE_ROOT=${CMAKE_SOURCE_DIR}" -P "${CMAKE_CURRENT_SOURCE_DIR}/check_boundary.cmake")
set_tests_properties(qindaqt.bluetooth-radio-boundary PROPERTIES LABELS "service;bluetooth;radio;policy")

add_test(NAME qindaqt.bluetooth-radio-staged-install COMMAND "${CMAKE_COMMAND}"
    "-DBINARY_ROOT=${CMAKE_BINARY_DIR}"
    "-DSTAGE_ROOT=${CMAKE_CURRENT_BINARY_DIR}/staged-install"
    "-DCONSUMER_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/installed_consumer"
    "-DRADIO_LIBRARY=$<TARGET_FILE_NAME:qindaqt_bluetooth_radio_client>"
    "-DINSTALL_LIBDIR=${CMAKE_INSTALL_LIBDIR}"
    "-DINSTALL_INCLUDEDIR=${CMAKE_INSTALL_INCLUDEDIR}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/check_installed.cmake")
set_tests_properties(qindaqt.bluetooth-radio-staged-install PROPERTIES
    LABELS "service;bluetooth;radio;package" TIMEOUT 60 RUN_SERIAL TRUE)

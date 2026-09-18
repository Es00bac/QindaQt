# SPDX-License-Identifier: GPL-3.0-or-later

find_program(QINDAQT_DBUS_DAEMON_EXECUTABLE NAMES dbus-daemon REQUIRED)

# Pure policy: matcher, ledger, calibration and area geometry. No bus, no
# device, no window — every rule here is a value decision.
qt_add_executable(qindaqt_tablet_policy_values_tests
    tst_tablet_policy_values.cpp)
target_link_libraries(qindaqt_tablet_policy_values_tests
    PRIVATE qindaqt_tablet_devices Qt6::Test)
qindaqt_enable_warnings(qindaqt_tablet_policy_values_tests)
add_test(NAME qindaqt.services-tablet-devices-policy-values
    COMMAND qindaqt_tablet_policy_values_tests)
set_tests_properties(qindaqt.services-tablet-devices-policy-values PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    LABELS "services;tablet-devices;model")

# The KWin tablet authority port and hotplug watcher against a fake
# org.kde.KWin on a private bus.
qt_add_executable(qindaqt_tablet_device_port_tests
    tst_tablet_device_port.cpp
    support/fake_kwin_tablets.h
    support/private_bus.h)
target_include_directories(qindaqt_tablet_device_port_tests
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(qindaqt_tablet_device_port_tests
    PRIVATE QINDAQT_DBUS_DAEMON_EXECUTABLE="${QINDAQT_DBUS_DAEMON_EXECUTABLE}")
target_link_libraries(qindaqt_tablet_device_port_tests
    PRIVATE qindaqt_tablet_devices Qt6::Core Qt6::DBus Qt6::Gui Qt6::Test)
qindaqt_enable_warnings(qindaqt_tablet_device_port_tests)
add_test(NAME qindaqt.services-tablet-devices-port
    COMMAND qindaqt_tablet_device_port_tests)
set_tests_properties(qindaqt.services-tablet-devices-port PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    LABELS "services;tablet-devices;dbus" TIMEOUT 60 RUN_SERIAL TRUE)

# The shared mapping store: the one ledger both the session policy and the
# Settings route write through.
qt_add_executable(qindaqt_tablet_mapping_store_tests tst_tablet_mapping_store.cpp)
target_link_libraries(qindaqt_tablet_mapping_store_tests
    PRIVATE qindaqt_tablet_devices Qt6::Test)
qindaqt_enable_warnings(qindaqt_tablet_mapping_store_tests)
add_test(NAME qindaqt.services-tablet-devices-mapping-store
    COMMAND qindaqt_tablet_mapping_store_tests)
set_tests_properties(qindaqt.services-tablet-devices-mapping-store PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    LABELS "services;tablet-devices;model")

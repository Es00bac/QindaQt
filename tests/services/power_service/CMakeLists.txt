# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_executable(
    qindaqt_power_service_publication_tests
    tst_power_service_publication.cpp
    support/fake_power_collaborators.h
)
target_link_libraries(
    qindaqt_power_service_publication_tests
    PRIVATE QindaQt::PowerService Qt6::DBus Qt6::Test
)
qindaqt_enable_warnings(qindaqt_power_service_publication_tests)
add_test(
    NAME qindaqt.power-service-publication
    COMMAND qindaqt_power_service_publication_tests
)
set_tests_properties(
    qindaqt.power-service-publication
    PROPERTIES LABELS "service;power" TIMEOUT 20
)

qt_add_executable(
    qindaqt_power_service_operation_tests
    tst_power_service_operations.cpp
    support/fake_power_collaborators.h
)
target_link_libraries(
    qindaqt_power_service_operation_tests
    PRIVATE QindaQt::PowerService Qt6::DBus Qt6::Test
)
qindaqt_enable_warnings(qindaqt_power_service_operation_tests)
add_test(
    NAME qindaqt.power-service-operations
    COMMAND qindaqt_power_service_operation_tests
)
set_tests_properties(
    qindaqt.power-service-operations
    PROPERTIES LABELS "service;power" TIMEOUT 20
)

qt_add_executable(
    qindaqt_power_service_residency_tests
    tst_power_service_residency.cpp
    support/fake_power_collaborators.h
)
target_link_libraries(
    qindaqt_power_service_residency_tests
    PRIVATE QindaQt::PowerService Qt6::DBus Qt6::Test
)
qindaqt_enable_warnings(qindaqt_power_service_residency_tests)
add_test(
    NAME qindaqt.power-service-residency
    COMMAND qindaqt_power_service_residency_tests
)
set_tests_properties(
    qindaqt.power-service-residency
    PROPERTIES LABELS "service;power;dbus" TIMEOUT 30
)

qt_add_executable(
    qindaqt_power_sysfs_backlight_tests
    tst_power_sysfs_backlight.cpp
)
target_link_libraries(
    qindaqt_power_sysfs_backlight_tests
    PRIVATE QindaQt::PowerService Qt6::Core Qt6::Test
)
target_compile_definitions(
    qindaqt_power_sysfs_backlight_tests
    PRIVATE QINDAQT_TEST_SCRATCH_DIR="${CMAKE_CURRENT_BINARY_DIR}/scratch"
)
qindaqt_enable_warnings(qindaqt_power_sysfs_backlight_tests)
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/scratch")
add_test(
    NAME qindaqt.power-service-sysfs-backlight
    COMMAND qindaqt_power_sysfs_backlight_tests
)
set_tests_properties(
    qindaqt.power-service-sysfs-backlight
    PROPERTIES LABELS "service;power" TIMEOUT 30
)

qt_add_executable(
    qindaqt_power_upstream_composition_tests
    tst_power_upstream_composition.cpp
)
target_link_libraries(
    qindaqt_power_upstream_composition_tests
    PRIVATE QindaQt::PowerService Qt6::Core Qt6::DBus Qt6::Test
)
qindaqt_enable_warnings(qindaqt_power_upstream_composition_tests)
add_test(
    NAME qindaqt.power-service-upstream-composition
    COMMAND qindaqt_power_upstream_composition_tests
)
set_tests_properties(
    qindaqt.power-service-upstream-composition
    PROPERTIES LABELS "service;power" TIMEOUT 20
)

qt_add_executable(
    qindaqt_power_upower_adapter_tests
    tst_power_upower_adapter.cpp
    support/fake_power_collaborators.h
    support/fake_upower_service.cpp
    support/fake_upower_service.h
    support/private_bus.h
)
target_link_libraries(
    qindaqt_power_upower_adapter_tests
    PRIVATE QindaQt::PowerService Qt6::DBus Qt6::Test
)
target_compile_definitions(
    qindaqt_power_upower_adapter_tests
    PRIVATE QINDAQT_TEST_SCRATCH_DIR="${CMAKE_CURRENT_BINARY_DIR}/scratch"
)
qindaqt_enable_warnings(qindaqt_power_upower_adapter_tests)
add_test(
    NAME qindaqt.power-service-upower-adapter
    COMMAND qindaqt_power_upower_adapter_tests
)
set_tests_properties(
    qindaqt.power-service-upower-adapter
    PROPERTIES LABELS "service;power;dbus" TIMEOUT 60
)

qt_add_executable(
    qindaqt_power_profiles_adapter_tests
    tst_power_profiles_adapter.cpp
    support/fake_power_collaborators.h
    support/fake_ppd_service.cpp
    support/fake_ppd_service.h
    support/private_bus.h
)
target_link_libraries(
    qindaqt_power_profiles_adapter_tests
    PRIVATE QindaQt::PowerService Qt6::DBus Qt6::Test
)
qindaqt_enable_warnings(qindaqt_power_profiles_adapter_tests)
add_test(
    NAME qindaqt.power-service-profiles-adapter
    COMMAND qindaqt_power_profiles_adapter_tests
)
set_tests_properties(
    qindaqt.power-service-profiles-adapter
    PROPERTIES LABELS "service;power;dbus" TIMEOUT 60
)

qt_add_executable(
    qindaqt_power_logind_adapter_tests
    tst_power_logind_adapter.cpp
    support/fake_power_collaborators.h
    support/fake_logind_service.cpp
    support/fake_logind_service.h
    support/private_bus.h
)
target_link_libraries(
    qindaqt_power_logind_adapter_tests
    PRIVATE QindaQt::PowerService Qt6::DBus Qt6::Test
)
qindaqt_enable_warnings(qindaqt_power_logind_adapter_tests)
add_test(
    NAME qindaqt.power-service-logind-adapter
    COMMAND qindaqt_power_logind_adapter_tests
)
set_tests_properties(
    qindaqt.power-service-logind-adapter
    PROPERTIES LABELS "service;power;dbus" TIMEOUT 60
)

qt_add_executable(
    qindaqt_power_logind_actions_tests
    tst_power_logind_actions.cpp
    support/fake_logind_service.cpp
    support/fake_logind_service.h
    support/private_bus.h
)
target_link_libraries(
    qindaqt_power_logind_actions_tests
    PRIVATE QindaQt::PowerService Qt6::DBus Qt6::Test
)
qindaqt_enable_warnings(qindaqt_power_logind_actions_tests)
add_test(
    NAME qindaqt.power-service-logind-actions
    COMMAND qindaqt_power_logind_actions_tests
)
set_tests_properties(
    qindaqt.power-service-logind-actions
    PROPERTIES LABELS "service;power;dbus" TIMEOUT 60
)

find_program(QINDAQT_DBUS_DAEMON_EXECUTABLE_TESTS NAMES dbus-daemon REQUIRED)
qt_add_executable(
    qindaqt_power_production_activation_tests
    tst_power_production_activation.cpp
    support/fake_logind_service.cpp
    support/fake_logind_service.h
    support/fake_ppd_service.cpp
    support/fake_ppd_service.h
    support/fake_upower_service.cpp
    support/fake_upower_service.h
)
target_link_libraries(
    qindaqt_power_production_activation_tests
    PRIVATE QindaQt::PowerClient QindaQt::PowerService Qt6::DBus Qt6::Test
)
target_compile_definitions(
    qindaqt_power_production_activation_tests
    PRIVATE
        QINDAQT_POWER_SERVICE_EXECUTABLE="$<TARGET_FILE:qindaqt-power-service>"
        QINDAQT_DBUS_DAEMON_EXECUTABLE="${QINDAQT_DBUS_DAEMON_EXECUTABLE_TESTS}"
        QINDAQT_DBUS_SERVICE_FILE="${CMAKE_BINARY_DIR}/src/services/power_service/org.qindaqt.Power1.service"
        QINDAQT_SYSTEMD_UNIT_FILE="${CMAKE_BINARY_DIR}/src/services/power_service/qindaqt-power-service.service"
        QINDAQT_TEST_SCRATCH_DIR="${CMAKE_CURRENT_BINARY_DIR}/scratch"
)
qindaqt_enable_warnings(qindaqt_power_production_activation_tests)
add_dependencies(qindaqt_power_production_activation_tests qindaqt-power-service)
add_test(
    NAME qindaqt.power-service-production-activation
    COMMAND qindaqt_power_production_activation_tests
)
set_tests_properties(
    qindaqt.power-service-production-activation
    PROPERTIES LABELS "service;power;dbus;activation" TIMEOUT 120 RUN_SERIAL TRUE
)

add_test(
    NAME qindaqt.power-service-boundary
    COMMAND
        "${CMAKE_COMMAND}"
        "-DSOURCE_ROOT=${CMAKE_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/check_boundary.cmake"
)
set_tests_properties(
    qindaqt.power-service-boundary
    PROPERTIES LABELS "service;power;policy"
)

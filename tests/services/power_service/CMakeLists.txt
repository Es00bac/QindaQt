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

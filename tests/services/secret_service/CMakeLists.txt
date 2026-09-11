# SPDX-License-Identifier: GPL-3.0-or-later

find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(QINDAQT_SECRET_SERVICE_SCENARIO
    "${CMAKE_CURRENT_SOURCE_DIR}/secret_service_scenario.py")
set(QINDAQT_SECRET_SERVICE_ENVIRONMENT
    "PYTHONDONTWRITEBYTECODE=1"
    "DBUS_SESSION_BUS_ADDRESS="
    "DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent"
)

add_test(
    NAME qindaqt.secret-service-provider
    COMMAND
        "${CMAKE_COMMAND}" -E env ${QINDAQT_SECRET_SERVICE_ENVIRONMENT}
        "${Python3_EXECUTABLE}" "${QINDAQT_SECRET_SERVICE_SCENARIO}"
        --case provider
        --work "${CMAKE_CURRENT_BINARY_DIR}/work-provider"
)
set_tests_properties(
    qindaqt.secret-service-provider
    PROPERTIES
        LABELS "service;secret-service;private-bus"
        TIMEOUT 300
        SKIP_RETURN_CODE 77
)

add_test(
    NAME qindaqt.secret-service-wrong-password
    COMMAND
        "${CMAKE_COMMAND}" -E env ${QINDAQT_SECRET_SERVICE_ENVIRONMENT}
        "${Python3_EXECUTABLE}" "${QINDAQT_SECRET_SERVICE_SCENARIO}"
        --case wrong-password
        --work "${CMAKE_CURRENT_BINARY_DIR}/work-wrong-password"
)
set_tests_properties(
    qindaqt.secret-service-wrong-password
    PROPERTIES
        LABELS "service;secret-service;private-bus"
        TIMEOUT 300
        SKIP_RETURN_CODE 77
)

add_test(
    NAME qindaqt.secret-service-no-daemon
    COMMAND
        "${CMAKE_COMMAND}" -E env ${QINDAQT_SECRET_SERVICE_ENVIRONMENT}
        "${Python3_EXECUTABLE}" "${QINDAQT_SECRET_SERVICE_SCENARIO}"
        --case no-daemon
        --work "${CMAKE_CURRENT_BINARY_DIR}/work-no-daemon"
)
set_tests_properties(
    qindaqt.secret-service-no-daemon
    PROPERTIES
        LABELS "service;secret-service;private-bus"
        TIMEOUT 300
        SKIP_RETURN_CODE 77
)

add_test(
    NAME qindaqt.keyring-check-parsing
    COMMAND
        "${CMAKE_COMMAND}" -E env
        "PYTHONDONTWRITEBYTECODE=1"
        "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/keyring_check_parsing.py"
)
set_tests_properties(
    qindaqt.keyring-check-parsing
    PROPERTIES LABELS "service;secret-service;unit" TIMEOUT 60
)

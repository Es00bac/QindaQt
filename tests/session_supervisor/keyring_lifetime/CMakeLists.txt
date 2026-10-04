# SPDX-License-Identifier: GPL-3.0-or-later
# A focused build compiles the same lifetime/child production sources; it does
# not replace admission with a fixture-specific implementation.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.25)
    project(QindaQtKeyringSessionLifetimeTests LANGUAGES CXX)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
    include(CTest)
    find_package(Qt6 6.11 REQUIRED COMPONENTS Core DBus Test)
    qt_standard_project_setup(REQUIRES 6.11)
    get_filename_component(QINDAQT_SOURCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
    set(QINDAQT_ENABLE_STRICT_WARNINGS ON)
    include("${QINDAQT_SOURCE_ROOT}/cmake/QindaQtCompiler.cmake")
    qt_add_library(qindaqt_keyring_lifetime_support STATIC
        "${QINDAQT_SOURCE_ROOT}/src/session_supervisor/src/keyring_session_lifetime.cpp"
        "${QINDAQT_SOURCE_ROOT}/src/session_supervisor/src/keyring_session_lifetime.h"
        "${QINDAQT_SOURCE_ROOT}/src/session_supervisor/src/session_optional_child.cpp"
        "${QINDAQT_SOURCE_ROOT}/src/session_supervisor/include/qindaqt/session_supervisor/session_optional_child.h"
        "${QINDAQT_SOURCE_ROOT}/src/session_supervisor/src/supervised_process_launcher.cpp")
    target_include_directories(qindaqt_keyring_lifetime_support PUBLIC
        "${QINDAQT_SOURCE_ROOT}/src/session_supervisor/include")
    target_link_libraries(qindaqt_keyring_lifetime_support PUBLIC Qt6::Core Qt6::DBus)
    add_library(QindaQt::SessionSupervisorSupport ALIAS qindaqt_keyring_lifetime_support)
    qindaqt_enable_warnings(qindaqt_keyring_lifetime_support)
else()
    set(QINDAQT_SOURCE_ROOT "${PROJECT_SOURCE_DIR}")
endif()
qt_add_executable(qindaqt_keyring_lifetime_endpoint keyring_endpoint.cpp)
target_link_libraries(qindaqt_keyring_lifetime_endpoint PRIVATE Qt6::Core Qt6::DBus)
qindaqt_enable_warnings(qindaqt_keyring_lifetime_endpoint)
qt_add_executable(qindaqt_keyring_lifetime_tests tst_keyring_session_lifetime.cpp)
target_include_directories(qindaqt_keyring_lifetime_tests PRIVATE "${QINDAQT_SOURCE_ROOT}")
target_link_libraries(qindaqt_keyring_lifetime_tests PRIVATE QindaQt::SessionSupervisorSupport Qt6::Test)
target_compile_definitions(qindaqt_keyring_lifetime_tests PRIVATE
    QINDAQT_KEYRING_LIFETIME_ENDPOINT="$<TARGET_FILE:qindaqt_keyring_lifetime_endpoint>")
add_dependencies(qindaqt_keyring_lifetime_tests qindaqt_keyring_lifetime_endpoint)
qindaqt_enable_warnings(qindaqt_keyring_lifetime_tests)
add_test(NAME qindaqt.session-keyring-lifetime COMMAND "${CMAKE_COMMAND}" -E env
    --unset=DBUS_SESSION_BUS_ADDRESS --unset=DBUS_STARTER_ADDRESS --unset=DBUS_STARTER_BUS_TYPE
    dbus-run-session --config-file=${CMAKE_CURRENT_SOURCE_DIR}/private-bus.conf
    -- $<TARGET_FILE:qindaqt_keyring_lifetime_tests>)
set_tests_properties(qindaqt.session-keyring-lifetime PROPERTIES TIMEOUT 45
    ENVIRONMENT "QT_FATAL_WARNINGS=1;DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent"
    LABELS "session;keyring;private-bus")
qt_add_executable(qindaqt_keyring_lifetime_owner keyring_owner.cpp)
target_include_directories(qindaqt_keyring_lifetime_owner PRIVATE "${QINDAQT_SOURCE_ROOT}")
target_link_libraries(qindaqt_keyring_lifetime_owner PRIVATE QindaQt::SessionSupervisorSupport)
qindaqt_enable_warnings(qindaqt_keyring_lifetime_owner)
if(TARGET qindaqt-keyring AND TARGET qindaqt-settings-service)
    add_dependencies(qindaqt_keyring_lifetime_owner qindaqt-keyring
        qindaqt-settings-service qindaqt_keyring_policy_compositor)
    set(QINDAQT_KEYRING_TEST_DAEMON "$<TARGET_FILE:qindaqt-keyring>")
    set(QINDAQT_KEYRING_TEST_COMPOSITOR "$<TARGET_FILE:qindaqt_keyring_policy_compositor>")
    set(QINDAQT_KEYRING_TEST_SETTINGS "$<TARGET_FILE:qindaqt-settings-service>")
endif()
if(QINDAQT_KEYRING_TEST_DAEMON AND QINDAQT_KEYRING_TEST_COMPOSITOR AND QINDAQT_KEYRING_TEST_SETTINGS)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    add_test(NAME qindaqt.session-keyring-owner-replacement COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/test_owner_replacement.py"
        "$<TARGET_FILE:qindaqt_keyring_lifetime_owner>" "${QINDAQT_KEYRING_TEST_DAEMON}"
        "${QINDAQT_KEYRING_TEST_COMPOSITOR}" "${QINDAQT_KEYRING_TEST_SETTINGS}"
        "${QINDAQT_SOURCE_ROOT}/data/settings")
    set_tests_properties(qindaqt.session-keyring-owner-replacement PROPERTIES TIMEOUT 40
        LABELS "session;keyring;private-bus;software-compositor")
endif()

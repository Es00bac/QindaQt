# SPDX-License-Identifier: GPL-3.0-or-later

# Runtime test executables link the static DisplayRuntime/DisplayWriter graph;
# declare the writer's Qt component in this directory so Qt's deferred target
# finalizer can walk the complete dependency graph without cross-scope gaps.
find_package(Qt6 6.11 REQUIRED COMPONENTS Core DBus Test WaylandClient)
find_program(QINDAQT_DISPLAY_RUNTIME_DBUS_DAEMON_EXECUTABLE dbus-daemon REQUIRED)

qt_add_executable(
    qindaqt_display_runtime_state_root_tests
    tst_state_root.cpp
)
target_link_libraries(
    qindaqt_display_runtime_state_root_tests
    PRIVATE QindaQt::DisplayRuntime Qt6::Core Qt6::Test
)
qindaqt_enable_warnings(qindaqt_display_runtime_state_root_tests)
add_test(
    NAME qindaqt.display-runtime-state-root
    COMMAND qindaqt_display_runtime_state_root_tests
)

qt_add_executable(
    qindaqt_resident_display_runtime_tests
    tst_resident_display_runtime.cpp
    support/display_runtime_test_support.h
    ../display_service/support/display_service_test_support.h
    ../display_service/support/private_bus_test_support.h
    ../display_writer/support/display_writer_test_support.h
)
target_link_libraries(
    qindaqt_resident_display_runtime_tests
    PRIVATE QindaQt::DisplayRuntime Qt6::Core Qt6::DBus Qt6::Test
)
target_compile_definitions(
    qindaqt_resident_display_runtime_tests
    PRIVATE
        QINDAQT_DBUS_DAEMON_EXECUTABLE="${QINDAQT_DISPLAY_RUNTIME_DBUS_DAEMON_EXECUTABLE}"
)
qindaqt_enable_warnings(qindaqt_resident_display_runtime_tests)
add_test(
    NAME qindaqt.display-runtime-resident-private-bus
    COMMAND qindaqt_resident_display_runtime_tests
)

qt_add_executable(
    qindaqt_session_safety_private_bus_tests
    tst_session_safety_private_bus.cpp
    ../display_service/support/private_bus_test_support.h
)
target_link_libraries(
    qindaqt_session_safety_private_bus_tests
    PRIVATE QindaQt::DisplayRuntime Qt6::Core Qt6::DBus Qt6::Test
)
target_compile_definitions(
    qindaqt_session_safety_private_bus_tests
    PRIVATE
        QINDAQT_DBUS_DAEMON_EXECUTABLE="${QINDAQT_DISPLAY_RUNTIME_DBUS_DAEMON_EXECUTABLE}"
)
qindaqt_enable_warnings(qindaqt_session_safety_private_bus_tests)
add_test(
    NAME qindaqt.display-runtime-session-safety-private-bus
    COMMAND qindaqt_session_safety_private_bus_tests
)

add_test(
    NAME qindaqt.display-runtime-boundary
    COMMAND
        "${CMAKE_COMMAND}"
        "-DSOURCE_ROOT=${CMAKE_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/check_boundary.cmake"
)
add_test(
    NAME qindaqt.display-runtime-boundary-poison
    COMMAND
        "${CMAKE_COMMAND}"
        "-DCHECK_SCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/check_boundary.cmake"
        "-DMODULE_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../../../src/services/display_runtime"
        "-DSTAGE_DIR=${CMAKE_CURRENT_BINARY_DIR}/boundary-poison-stage"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/check_boundary_negative.cmake"
)
add_test(
    NAME qindaqt.display-runtime-installed-boundary-poison
    COMMAND
        "${CMAKE_COMMAND}"
        "-DQINDAQT_CMAKE=${CMAKE_COMMAND}"
        "-DQINDAQT_BUILD_DIRECTORY=${CMAKE_BINARY_DIR}"
        "-DQINDAQT_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/installed-stage"
        "-DQINDAQT_INSTALL_INCLUDEDIR=${CMAKE_INSTALL_INCLUDEDIR}"
        "-DQINDAQT_SOURCE_ROOT=${CMAKE_SOURCE_DIR}"
        "-DQINDAQT_CONSUMER_SOURCE_DIRECTORY=${CMAKE_CURRENT_SOURCE_DIR}/installed_consumer"
        "-DQINDAQT_QT6_DIR=${Qt6_DIR}"
        "-DQINDAQT_GENERATOR=${CMAKE_GENERATOR}"
        "-DQINDAQT_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
        "-DQINDAQT_CONFIGURATION=$<CONFIG>"
        "-DCHECK_SCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/check_installed_boundary.cmake"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_installed_boundary.cmake"
)
add_test(
    NAME qindaqt.display-runtime-process-startup
    COMMAND
        "${CMAKE_COMMAND}"
        "-DQINDAQT_DISPLAY_SERVICE=$<TARGET_FILE:qindaqt-display-service>"
        "-DSTAGE_DIR=${CMAKE_CURRENT_BINARY_DIR}/process-startup-stage"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/check_process_startup.cmake"
)

set_tests_properties(
    qindaqt.display-runtime-state-root
    qindaqt.display-runtime-boundary
    qindaqt.display-runtime-boundary-poison
    qindaqt.display-runtime-process-startup
    PROPERTIES LABELS "service;display;policy" TIMEOUT 20
)
set_tests_properties(
    qindaqt.display-runtime-resident-private-bus
    qindaqt.display-runtime-session-safety-private-bus
    PROPERTIES
        LABELS "service;display;private-dbus;isolated-runtime"
        TIMEOUT 20
        RUN_SERIAL TRUE
)
set_tests_properties(
    qindaqt.display-runtime-installed-boundary-poison
    PROPERTIES
        LABELS "package;service;display;policy;hostile"
        RUN_SERIAL TRUE
        TIMEOUT 90
)

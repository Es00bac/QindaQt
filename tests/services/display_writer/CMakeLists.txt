# SPDX-License-Identifier: GPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Core Test WaylandClient)

function(qindaqt_add_display_writer_test target source test_name)
    qt_add_executable(${target} ${source} support/display_writer_test_support.h)
    target_link_libraries(
        ${target}
        PRIVATE QindaQt::DisplayWriter QindaQt::DisplayTopology Qt6::Core Qt6::Test
    )
    qindaqt_enable_warnings(${target})
    add_test(NAME ${test_name} COMMAND ${target})
    set_tests_properties(${test_name} PROPERTIES LABELS "service;display;writer" TIMEOUT 20)
endfunction()

qindaqt_add_display_writer_test(
    qindaqt_display_writer_mapper_tests
    tst_display_writer_mapper.cpp
    qindaqt.display-writer-mapper
)
qindaqt_add_display_writer_test(
    qindaqt_display_writer_port_tests
    tst_display_writer_port.cpp
    qindaqt.display-writer-port
)
qindaqt_add_display_writer_test(
    qindaqt_display_writer_brightness_port_tests
    tst_display_writer_brightness_port.cpp
    qindaqt.display-writer-brightness-port
)

# Reuse the test-only Wayland server dependency already used by clipboard.
find_package(Wayland REQUIRED COMPONENTS Server)
find_package(WaylandScanner REQUIRED)
enable_language(C)
qt_add_executable(qindaqt_display_writer_wayland_startup_tests
    tst_display_writer_wayland_startup.cpp)
foreach(protocol IN ITEMS kde-output-device-v2 kde-output-management-v2)
    ecm_add_wayland_server_protocol(qindaqt_display_writer_wayland_startup_tests
        PROTOCOL "${CMAKE_SOURCE_DIR}/src/services/display_writer/protocol/${protocol}.xml"
        BASENAME "${protocol}" PRIVATE_CODE)
    set_source_files_properties("${CMAKE_CURRENT_BINARY_DIR}/wayland-${protocol}-protocol.c"
        PROPERTIES COMPILE_OPTIONS "-Wno-conversion;-Wno-sign-conversion;-Wno-shadow")
endforeach()
target_include_directories(qindaqt_display_writer_wayland_startup_tests
    PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(qindaqt_display_writer_wayland_startup_tests
    PRIVATE QindaQt::DisplayWriter Wayland::Server Qt6::Test)
qindaqt_enable_warnings(qindaqt_display_writer_wayland_startup_tests)
add_test(NAME qindaqt.display-writer-wayland-startup
    COMMAND qindaqt_display_writer_wayland_startup_tests)
set_tests_properties(qindaqt.display-writer-wayland-startup
    PROPERTIES LABELS "service;display;writer;wayland" TIMEOUT 15)

# ADR-0150 private brightness peer. It reuses the protocol code generated for
# the startup row in this directory instead of emitting a second custom rule.
qt_add_executable(qindaqt_display_writer_wayland_brightness_tests
    tst_display_writer_wayland_brightness.cpp
    support/display_writer_test_support.h
    "${CMAKE_CURRENT_BINARY_DIR}/wayland-kde-output-device-v2-protocol.c"
    "${CMAKE_CURRENT_BINARY_DIR}/wayland-kde-output-device-v2-server-protocol.h"
    "${CMAKE_CURRENT_BINARY_DIR}/wayland-kde-output-management-v2-protocol.c"
    "${CMAKE_CURRENT_BINARY_DIR}/wayland-kde-output-management-v2-server-protocol.h")
target_include_directories(qindaqt_display_writer_wayland_brightness_tests
    PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(qindaqt_display_writer_wayland_brightness_tests
    PRIVATE QindaQt::DisplayWriter Wayland::Server Qt6::Test)
qindaqt_enable_warnings(qindaqt_display_writer_wayland_brightness_tests)
add_test(NAME qindaqt.display-writer-wayland-brightness
    COMMAND qindaqt_display_writer_wayland_brightness_tests)
set_tests_properties(qindaqt.display-writer-wayland-brightness
    PROPERTIES LABELS "service;display;writer;wayland" TIMEOUT 20)

# ADR-0150 private nested-KWin proof helper (no test registration). It runs
# only through proof/run_brightness_nested_proof.sh against a disposable
# kwin_wayland --virtual socket, never as a ctest row. Only client headers are
# generated here; the interface definitions come from the writer archive.
find_package(Wayland REQUIRED COMPONENTS Client)
set(QINDAQT_DISPLAY_BRIGHTNESS_PROOF_DIR "${CMAKE_CURRENT_BINARY_DIR}/brightness-proof")
foreach(protocol IN ITEMS kde-output-device-v2 kde-output-management-v2)
    add_custom_command(
        OUTPUT "${QINDAQT_DISPLAY_BRIGHTNESS_PROOF_DIR}/wayland-${protocol}-client-protocol.h"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${QINDAQT_DISPLAY_BRIGHTNESS_PROOF_DIR}"
        COMMAND "${WaylandScanner_EXECUTABLE}" client-header
            "${CMAKE_SOURCE_DIR}/src/services/display_writer/protocol/${protocol}.xml"
            "${QINDAQT_DISPLAY_BRIGHTNESS_PROOF_DIR}/wayland-${protocol}-client-protocol.h"
        DEPENDS "${CMAKE_SOURCE_DIR}/src/services/display_writer/protocol/${protocol}.xml"
        VERBATIM)
endforeach()
qt_add_executable(qindaqt_display_brightness_nested_proof_helper
    proof/brightness_nested_proof_helper.cpp
    "${QINDAQT_DISPLAY_BRIGHTNESS_PROOF_DIR}/wayland-kde-output-device-v2-client-protocol.h"
    "${QINDAQT_DISPLAY_BRIGHTNESS_PROOF_DIR}/wayland-kde-output-management-v2-client-protocol.h")
target_include_directories(qindaqt_display_brightness_nested_proof_helper
    PRIVATE "${QINDAQT_DISPLAY_BRIGHTNESS_PROOF_DIR}")
target_link_libraries(qindaqt_display_brightness_nested_proof_helper
    PRIVATE QindaQt::DisplayWriter Wayland::Client Qt6::Core)
qindaqt_enable_warnings(qindaqt_display_brightness_nested_proof_helper)

add_test(
    NAME qindaqt.display-writer-boundary
    COMMAND
        "${CMAKE_COMMAND}"
        "-DSOURCE_ROOT=${CMAKE_SOURCE_DIR}"
        "-DEXPECTED_DEVICE_SHA256=52f8dc89df7ea6b6fe3930ff5d215aadb0841b6e1bc4e3cc9335d8745649da84"
        "-DEXPECTED_MANAGEMENT_SHA256=07582b4596e18b557d5ee6b22f35d2c4304fbd5bf5bdc65eb29c69a18ebac5dc"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/check_boundary.cmake"
)
set_tests_properties(
    qindaqt.display-writer-boundary
    PROPERTIES LABELS "service;display;writer;policy" TIMEOUT 20
)

add_test(
    NAME qindaqt.display-writer-boundary-poison
    COMMAND
        "${CMAKE_COMMAND}"
        "-DCHECK_SCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/check_boundary.cmake"
        "-DMODULE_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../../../src/services/display_writer"
        "-DSTAGE_DIR=${CMAKE_CURRENT_BINARY_DIR}/boundary-poison-stage"
        "-DEXPECTED_DEVICE_SHA256=52f8dc89df7ea6b6fe3930ff5d215aadb0841b6e1bc4e3cc9335d8745649da84"
        "-DEXPECTED_MANAGEMENT_SHA256=07582b4596e18b557d5ee6b22f35d2c4304fbd5bf5bdc65eb29c69a18ebac5dc"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/check_boundary_negative.cmake"
)
set_tests_properties(
    qindaqt.display-writer-boundary-poison
    PROPERTIES LABELS "service;display;writer;policy;hostile" TIMEOUT 20
)

add_test(
    NAME qindaqt.display-writer-installed-boundary-poison
    COMMAND
        "${CMAKE_COMMAND}"
        "-DQINDAQT_CMAKE=${CMAKE_COMMAND}"
        "-DQINDAQT_BUILD_DIRECTORY=${CMAKE_BINARY_DIR}"
        "-DQINDAQT_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/installed-stage"
        "-DQINDAQT_INSTALL_INCLUDEDIR=${CMAKE_INSTALL_INCLUDEDIR}"
        "-DQINDAQT_SOURCE_ROOT=${CMAKE_SOURCE_DIR}"
        "-DQINDAQT_CONSUMER_SOURCE_DIRECTORY=${CMAKE_CURRENT_SOURCE_DIR}/installed_consumer"
        "-DQINDAQT_CONSUMER_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/installed_consumer/installed_header_consumer.cpp"
        "-DQINDAQT_QT6_DIR=${Qt6_DIR}"
        "-DQINDAQT_GENERATOR=${CMAKE_GENERATOR}"
        "-DQINDAQT_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
        "-DQINDAQT_CONFIGURATION=$<CONFIG>"
        "-DCHECK_SCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/check_installed_boundary.cmake"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_installed_boundary.cmake"
)
set_tests_properties(
    qindaqt.display-writer-installed-boundary-poison
    PROPERTIES LABELS "package;service;display;writer;policy;hostile"
               RUN_SERIAL TRUE TIMEOUT 90
)

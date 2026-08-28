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

# SPDX-License-Identifier: GPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Core Test)

qt_add_executable(
    qindaqt_color_discovery_tests
    tst_profile_discovery.cpp
    support/icc_file_builder.h
)
target_include_directories(
    qindaqt_color_discovery_tests
    PRIVATE "${CMAKE_SOURCE_DIR}/src/services/display_color_discovery/src"
)
target_link_libraries(
    qindaqt_color_discovery_tests
    PRIVATE QindaQt::DisplayColorDiscovery QindaQt::DisplayColorModel Qt6::Core Qt6::Test
)
qindaqt_enable_warnings(qindaqt_color_discovery_tests)
add_test(
    NAME qindaqt.display-color-discovery
    COMMAND qindaqt_color_discovery_tests
)
set_tests_properties(
    qindaqt.display-color-discovery
    PROPERTIES LABELS "service;display;color;filesystem"
)

qt_add_executable(
    qindaqt_color_import_tests
    tst_profile_import.cpp
    support/icc_file_builder.h
)
target_include_directories(
    qindaqt_color_import_tests
    PRIVATE "${CMAKE_SOURCE_DIR}/src/services/display_color_discovery/src"
)
target_link_libraries(
    qindaqt_color_import_tests
    PRIVATE QindaQt::DisplayColorDiscovery QindaQt::DisplayColorModel Qt6::Core Qt6::Test
)
qindaqt_enable_warnings(qindaqt_color_import_tests)
add_test(
    NAME qindaqt.display-color-discovery-import
    COMMAND qindaqt_color_import_tests
)
set_tests_properties(
    qindaqt.display-color-discovery-import
    PROPERTIES LABELS "service;display;color;filesystem"
)

add_test(
    NAME qindaqt.display-color-discovery-boundary
    COMMAND
        "${CMAKE_COMMAND}"
        "-DSOURCE_ROOT=${CMAKE_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/check_boundary.cmake"
)
set_tests_properties(
    qindaqt.display-color-discovery-boundary
    PROPERTIES LABELS "service;display;color;filesystem;policy"
)

# Poison negative: proves the boundary policy above is not vacuous by
# planting a forbidden dependency in a disposable copy and requiring the
# check to reject it.
add_test(
    NAME qindaqt.display-color-discovery-boundary-poison
    COMMAND
        "${CMAKE_COMMAND}"
        "-DCHECK_SCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/check_boundary.cmake"
        "-DMODULE_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../../../src/services/display_color_discovery"
        "-DSTAGE_DIR=${CMAKE_CURRENT_BINARY_DIR}/discovery-boundary-poison-stage"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/check_boundary_negative.cmake"
)
set_tests_properties(
    qindaqt.display-color-discovery-boundary-poison
    PROPERTIES LABELS "service;display;color;filesystem;policy"
)

add_test(
    NAME qindaqt.display-color-discovery-installed-cpp-consumer
    COMMAND
        "${CMAKE_COMMAND}"
        "-DQINDAQT_CMAKE=${CMAKE_COMMAND}"
        "-DQINDAQT_BUILD_DIRECTORY=${CMAKE_BINARY_DIR}"
        "-DQINDAQT_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/discovery-installed-cpp-stage"
        "-DQINDAQT_CONSUMER_SOURCE_DIRECTORY=${CMAKE_CURRENT_SOURCE_DIR}/installed_consumer"
        "-DQINDAQT_CONSUMER_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/installed_consumer/installed_cpp_consumer.cpp"
        "-DQINDAQT_INSTALL_LIBDIR=${CMAKE_INSTALL_LIBDIR}"
        "-DQINDAQT_INSTALL_INCLUDEDIR=${CMAKE_INSTALL_INCLUDEDIR}"
        "-DQINDAQT_DISPLAY_COLOR_DISCOVERY_LIBRARY_NAME=$<TARGET_FILE_NAME:qindaqt_display_color_discovery>"
        "-DQINDAQT_DISPLAY_COLOR_MODEL_LIBRARY_NAME=$<TARGET_FILE_NAME:qindaqt_display_color_model>"
        "-DQINDAQT_QT6_DIR=${Qt6_DIR}"
        "-DQINDAQT_GENERATOR=${CMAKE_GENERATOR}"
        "-DQINDAQT_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
        "-DQINDAQT_CONFIGURATION=$<CONFIG>"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_installed_cpp_consumer.cmake"
)
set_tests_properties(
    qindaqt.display-color-discovery-installed-cpp-consumer
    PROPERTIES LABELS "package;service;display;color"
)

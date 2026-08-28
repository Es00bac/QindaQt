# SPDX-License-Identifier: GPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Core Test)

qt_add_executable(
    qindaqt_color_header_validation_tests
    tst_color_header_validation.cpp
    support/color_test_data.h
)
target_link_libraries(
    qindaqt_color_header_validation_tests
    PRIVATE QindaQt::DisplayColorModel Qt6::Core Qt6::Test
)
qindaqt_enable_warnings(qindaqt_color_header_validation_tests)
add_test(
    NAME qindaqt.display-color-header-validation
    COMMAND qindaqt_color_header_validation_tests
)
set_tests_properties(
    qindaqt.display-color-header-validation
    PROPERTIES LABELS "service;display;color;pure"
)

qt_add_executable(
    qindaqt_color_catalog_tests
    tst_color_catalog.cpp
    support/color_test_data.h
)
target_link_libraries(
    qindaqt_color_catalog_tests
    PRIVATE QindaQt::DisplayColorModel Qt6::Core Qt6::Test
)
qindaqt_enable_warnings(qindaqt_color_catalog_tests)
add_test(
    NAME qindaqt.display-color-catalog
    COMMAND qindaqt_color_catalog_tests
)
set_tests_properties(
    qindaqt.display-color-catalog
    PROPERTIES LABELS "service;display;color;pure"
)

qt_add_executable(
    qindaqt_color_model_tests
    tst_color_model.cpp
    support/color_test_data.h
)
target_link_libraries(
    qindaqt_color_model_tests
    PRIVATE QindaQt::DisplayColorModel Qt6::Core Qt6::Test
)
qindaqt_enable_warnings(qindaqt_color_model_tests)
add_test(
    NAME qindaqt.display-color-model
    COMMAND qindaqt_color_model_tests
)
set_tests_properties(
    qindaqt.display-color-model
    PROPERTIES LABELS "service;display;color;pure"
)

add_test(
    NAME qindaqt.display-color-model-boundary
    COMMAND
        "${CMAKE_COMMAND}"
        "-DSOURCE_ROOT=${CMAKE_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/check_boundary.cmake"
)
set_tests_properties(
    qindaqt.display-color-model-boundary
    PROPERTIES LABELS "service;display;color;pure;policy"
)

# Poison negative: proves the boundary policy above is not vacuous by
# planting a forbidden dependency in a disposable copy and requiring the
# check to reject it.
add_test(
    NAME qindaqt.display-color-model-boundary-poison
    COMMAND
        "${CMAKE_COMMAND}"
        "-DCHECK_SCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/check_boundary.cmake"
        "-DMODULE_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../../../src/services/display_color_model"
        "-DSTAGE_DIR=${CMAKE_CURRENT_BINARY_DIR}/boundary-poison-stage"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/check_boundary_negative.cmake"
)
set_tests_properties(
    qindaqt.display-color-model-boundary-poison
    PROPERTIES LABELS "service;display;color;pure;policy"
)

add_test(
    NAME qindaqt.display-color-model-installed-cpp-consumer
    COMMAND
        "${CMAKE_COMMAND}"
        "-DQINDAQT_CMAKE=${CMAKE_COMMAND}"
        "-DQINDAQT_BUILD_DIRECTORY=${CMAKE_BINARY_DIR}"
        "-DQINDAQT_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/installed-cpp-stage"
        "-DQINDAQT_CONSUMER_SOURCE_DIRECTORY=${CMAKE_CURRENT_SOURCE_DIR}/installed_consumer"
        "-DQINDAQT_CONSUMER_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/installed_consumer/installed_cpp_consumer.cpp"
        "-DQINDAQT_INSTALL_LIBDIR=${CMAKE_INSTALL_LIBDIR}"
        "-DQINDAQT_INSTALL_INCLUDEDIR=${CMAKE_INSTALL_INCLUDEDIR}"
        "-DQINDAQT_DISPLAY_COLOR_MODEL_LIBRARY_NAME=$<TARGET_FILE_NAME:qindaqt_display_color_model>"
        "-DQINDAQT_QT6_DIR=${Qt6_DIR}"
        "-DQINDAQT_GENERATOR=${CMAKE_GENERATOR}"
        "-DQINDAQT_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
        "-DQINDAQT_CONFIGURATION=$<CONFIG>"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_installed_cpp_consumer.cmake"
)
set_tests_properties(
    qindaqt.display-color-model-installed-cpp-consumer
    PROPERTIES LABELS "package;service;display;color"
)

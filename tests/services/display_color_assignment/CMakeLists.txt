# SPDX-License-Identifier: GPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Core Test)

qt_add_executable(
    qindaqt_color_assignment_document_tests
    tst_assignment_document.cpp
)
target_link_libraries(
    qindaqt_color_assignment_document_tests
    PRIVATE QindaQt::DisplayColorAssignment QindaQt::DisplayColorModel Qt6::Core Qt6::Test
)
qindaqt_enable_warnings(qindaqt_color_assignment_document_tests)
add_test(
    NAME qindaqt.display-color-assignment-document
    COMMAND qindaqt_color_assignment_document_tests
)
set_tests_properties(
    qindaqt.display-color-assignment-document
    PROPERTIES LABELS "service;display;color;settings"
)

qt_add_executable(
    qindaqt_color_assignment_store_tests
    tst_assignment_store.cpp
)
target_link_libraries(
    qindaqt_color_assignment_store_tests
    PRIVATE QindaQt::DisplayColorAssignment QindaQt::SettingsClient Qt6::Core Qt6::Test
)
qindaqt_enable_warnings(qindaqt_color_assignment_store_tests)
add_test(
    NAME qindaqt.display-color-assignment-store
    COMMAND qindaqt_color_assignment_store_tests
)
set_tests_properties(
    qindaqt.display-color-assignment-store
    PROPERTIES LABELS "service;display;color;settings"
)

add_test(
    NAME qindaqt.display-color-assignment-boundary
    COMMAND
        "${CMAKE_COMMAND}"
        "-DSOURCE_ROOT=${CMAKE_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/check_boundary.cmake"
)
set_tests_properties(
    qindaqt.display-color-assignment-boundary
    PROPERTIES LABELS "service;display;color;settings;policy"
)

# Poison negative: proves the boundary policy above is not vacuous by
# planting a forbidden dependency in a disposable copy and requiring the
# check to reject it.
add_test(
    NAME qindaqt.display-color-assignment-boundary-poison
    COMMAND
        "${CMAKE_COMMAND}"
        "-DCHECK_SCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/check_boundary.cmake"
        "-DMODULE_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../../../src/services/display_color_assignment"
        "-DSTAGE_DIR=${CMAKE_CURRENT_BINARY_DIR}/assignment-boundary-poison-stage"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/check_boundary_negative.cmake"
)
set_tests_properties(
    qindaqt.display-color-assignment-boundary-poison
    PROPERTIES LABELS "service;display;color;settings;policy"
)

add_test(
    NAME qindaqt.display-color-assignment-installed-cpp-consumer
    COMMAND
        "${CMAKE_COMMAND}"
        "-DQINDAQT_CMAKE=${CMAKE_COMMAND}"
        "-DQINDAQT_BUILD_DIRECTORY=${CMAKE_BINARY_DIR}"
        "-DQINDAQT_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/assignment-installed-cpp-stage"
        "-DQINDAQT_CONSUMER_SOURCE_DIRECTORY=${CMAKE_CURRENT_SOURCE_DIR}/installed_consumer"
        "-DQINDAQT_CONSUMER_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/installed_consumer/installed_cpp_consumer.cpp"
        "-DQINDAQT_INSTALL_LIBDIR=${CMAKE_INSTALL_LIBDIR}"
        "-DQINDAQT_INSTALL_INCLUDEDIR=${CMAKE_INSTALL_INCLUDEDIR}"
        "-DQINDAQT_DISPLAY_COLOR_ASSIGNMENT_LIBRARY_NAME=$<TARGET_FILE_NAME:qindaqt_display_color_assignment>"
        "-DQINDAQT_DISPLAY_COLOR_MODEL_LIBRARY_NAME=$<TARGET_FILE_NAME:qindaqt_display_color_model>"
        "-DQINDAQT_SETTINGS_CLIENT_LIBRARY_NAME=$<TARGET_FILE_NAME:qindaqt_settings_client>"
        "-DQINDAQT_SETTINGS_PROTOCOL_LIBRARY_NAME=$<TARGET_FILE_NAME:qindaqt_settings_protocol>"
        "-DQINDAQT_QT6_DIR=${Qt6_DIR}"
        "-DQINDAQT_GENERATOR=${CMAKE_GENERATOR}"
        "-DQINDAQT_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
        "-DQINDAQT_CONFIGURATION=$<CONFIG>"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_installed_cpp_consumer.cmake"
)
set_tests_properties(
    qindaqt.display-color-assignment-installed-cpp-consumer
    PROPERTIES LABELS "package;service;display;color"
)

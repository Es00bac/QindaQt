# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_executable(qindaqt_network_client_tests tst_network_client.cpp)
target_include_directories(qindaqt_network_client_tests
                           PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../network_protocol")
target_link_libraries(qindaqt_network_client_tests
                      PRIVATE QindaQt::NetworkClient Qt6::Test)
qindaqt_enable_warnings(qindaqt_network_client_tests)
add_test(NAME qindaqt.network-client COMMAND qindaqt_network_client_tests)
set_tests_properties(qindaqt.network-client PROPERTIES LABELS "service;network;pure")

add_test(
    NAME qindaqt.network-installed-header-consumer
    COMMAND
        "${CMAKE_COMMAND}"
        "-DQINDAQT_CMAKE=${CMAKE_COMMAND}"
        "-DQINDAQT_BUILD_DIRECTORY=${CMAKE_BINARY_DIR}"
        "-DQINDAQT_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/installed-stage"
        "-DQINDAQT_CONSUMER_SOURCE_DIRECTORY=${CMAKE_CURRENT_SOURCE_DIR}/installed_consumer"
        "-DQINDAQT_CONSUMER_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/installed_cpp_consumer.cpp"
        "-DQINDAQT_INSTALL_LIBDIR=${CMAKE_INSTALL_LIBDIR}"
        "-DQINDAQT_INSTALL_INCLUDEDIR=${CMAKE_INSTALL_INCLUDEDIR}"
        "-DQINDAQT_NETWORK_PROTOCOL_LIBRARY_NAME=$<TARGET_FILE_NAME:qindaqt_network_protocol>"
        "-DQINDAQT_NETWORK_MODEL_LIBRARY_NAME=$<TARGET_FILE_NAME:qindaqt_network_model>"
        "-DQINDAQT_NETWORK_CLIENT_LIBRARY_NAME=$<TARGET_FILE_NAME:qindaqt_network_client>"
        "-DQINDAQT_QT6_DIR=${Qt6_DIR}"
        "-DQINDAQT_GENERATOR=${CMAKE_GENERATOR}"
        "-DQINDAQT_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
        "-DQINDAQT_CONFIGURATION=$<CONFIG>"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_installed_network_consumer.cmake"
)
set_tests_properties(
    qindaqt.network-installed-header-consumer
    PROPERTIES LABELS "package;network"
)

add_test(
    NAME qindaqt.network-boundary
    COMMAND
        "${CMAKE_COMMAND}"
        "-DSOURCE_ROOT=${CMAKE_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/check_boundary.cmake"
)
set_tests_properties(qindaqt.network-boundary
                     PROPERTIES LABELS "service;network;pure;policy")

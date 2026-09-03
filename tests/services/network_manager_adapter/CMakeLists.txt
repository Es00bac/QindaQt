# SPDX-License-Identifier: GPL-3.0-or-later

find_package(PkgConfig REQUIRED)
pkg_check_modules(NetworkManagerTest REQUIRED IMPORTED_TARGET libnm>=1.44)

qt_add_executable(
    qindaqt_network_manager_adapter_tests
    tst_network_manager_adapter.cpp
)
target_link_libraries(
    qindaqt_network_manager_adapter_tests
    PRIVATE
        QindaQt::NetworkManagerAdapter
        Qt6::DBus
        Qt6::Test
        PkgConfig::NetworkManagerTest
)
target_include_directories(
    qindaqt_network_manager_adapter_tests
    PRIVATE "${CMAKE_SOURCE_DIR}/src/services/network_manager_adapter/src"
)
target_compile_definitions(
    qindaqt_network_manager_adapter_tests PRIVATE QT_NO_KEYWORDS
)
qindaqt_enable_warnings(qindaqt_network_manager_adapter_tests)
add_test(
    NAME qindaqt.network-networkmanager-adapter
    COMMAND qindaqt_network_manager_adapter_tests
)
set_tests_properties(
    qindaqt.network-networkmanager-adapter
    PROPERTIES LABELS "service;network;networkmanager" TIMEOUT 20
)

qt_add_executable(
    qindaqt_network_manager_visible_profile_tests
    tst_network_visible_profile.cpp
)
target_link_libraries(
    qindaqt_network_manager_visible_profile_tests
    PRIVATE QindaQt::NetworkManagerAdapter Qt6::Test
            PkgConfig::NetworkManagerTest
)
target_include_directories(
    qindaqt_network_manager_visible_profile_tests
    PRIVATE "${CMAKE_SOURCE_DIR}/src/services/network_manager_adapter/src"
)
target_compile_definitions(
    qindaqt_network_manager_visible_profile_tests PRIVATE QT_NO_KEYWORDS
)
qindaqt_enable_warnings(qindaqt_network_manager_visible_profile_tests)
add_test(
    NAME qindaqt.network-networkmanager-visible-profile
    COMMAND qindaqt_network_manager_visible_profile_tests
)
set_tests_properties(
    qindaqt.network-networkmanager-visible-profile
    PROPERTIES LABELS "service;network;networkmanager;secret-free"
)

add_test(
    NAME qindaqt.network-n1-boundary
    COMMAND
        "${CMAKE_COMMAND}"
        "-DSOURCE_ROOT=${CMAKE_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/check_boundary.cmake"
)
set_tests_properties(
    qindaqt.network-n1-boundary
    PROPERTIES LABELS "service;network;networkmanager;policy"
)

add_test(
    NAME qindaqt.network-n1-boundary-poison
    COMMAND
        "${CMAKE_COMMAND}"
        "-DBINARY_ROOT=${CMAKE_BINARY_DIR}"
        "-DPOISON_ROOT=${CMAKE_CURRENT_BINARY_DIR}/boundary-poison"
        "-DCHECK_SCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/check_boundary.cmake"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/check_boundary_negative.cmake"
)
set_tests_properties(
    qindaqt.network-n1-boundary-poison
    PROPERTIES LABELS "service;network;networkmanager;policy;hostile"
)

add_test(
    NAME qindaqt.network-installed-package
    COMMAND
        "${CMAKE_COMMAND}"
        "-DQINDAQT_CMAKE=${CMAKE_COMMAND}"
        "-DQINDAQT_BUILD_DIRECTORY=${CMAKE_BINARY_DIR}"
        "-DQINDAQT_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/installed-network-stage"
        "-DQINDAQT_CONSUMER_SOURCE_DIRECTORY=${CMAKE_CURRENT_SOURCE_DIR}/installed_consumer"
        "-DQINDAQT_INSTALL_LIBDIR=${CMAKE_INSTALL_LIBDIR}"
        "-DQINDAQT_INSTALL_INCLUDEDIR=${CMAKE_INSTALL_INCLUDEDIR}"
        "-DQINDAQT_NETWORK_PROTOCOL_LIBRARY_NAME=$<TARGET_FILE_NAME:qindaqt_network_protocol>"
        "-DQINDAQT_NETWORK_MODEL_LIBRARY_NAME=$<TARGET_FILE_NAME:qindaqt_network_model>"
        "-DQINDAQT_NETWORK_CLIENT_LIBRARY_NAME=$<TARGET_FILE_NAME:qindaqt_network_client>"
        "-DQINDAQT_NETWORK_QT_TRANSPORT_LIBRARY_NAME=$<TARGET_FILE_NAME:qindaqt_network_qt_transport>"
        "-DQINDAQT_NETWORK_SERVICE_LIBRARY_NAME=$<TARGET_FILE_NAME:qindaqt_network_service>"
        "-DQINDAQT_NETWORK_MANAGER_ADAPTER_LIBRARY_NAME=$<TARGET_FILE_NAME:qindaqt_network_manager_adapter>"
        "-DQINDAQT_NETWORK_ACTIVATION_TEST=$<TARGET_FILE:qindaqt_network_activation_tests>"
        "-DQINDAQT_QT6_DIR=${Qt6_DIR}"
        "-DQINDAQT_GENERATOR=${CMAKE_GENERATOR}"
        "-DQINDAQT_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
        "-DQINDAQT_CONFIGURATION=$<CONFIG>"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_installed_network_package.cmake"
)
set_tests_properties(
    qindaqt.network-installed-package
    PROPERTIES
        LABELS "service;network;networkmanager;installed;activation"
        TIMEOUT 120
        RUN_SERIAL TRUE
)

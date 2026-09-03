# SPDX-License-Identifier: GPL-3.0-or-later

function(qindaqt_add_font_discovery_test target source test_name)
    qt_add_executable(${target} ${source})
    target_link_libraries(
        ${target}
        PRIVATE QindaQt::FontDiscovery Qt6::Test
    )
    target_compile_definitions(
        ${target}
        PRIVATE QINDAQT_FONT_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
    )
    target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    qindaqt_enable_warnings(${target})
    add_test(NAME ${test_name} COMMAND ${target})
    set_tests_properties(${test_name} PROPERTIES LABELS "service;fonts;discovery")
endfunction()

qindaqt_add_font_discovery_test(
    qindaqt_font_discovery_tests
    tst_font_discovery.cpp
    qindaqt.font-discovery
)
qindaqt_add_font_discovery_test(
    qindaqt_font_discovery_hostile_tests
    tst_font_discovery_hostile.cpp
    qindaqt.font-discovery-hostile
)
qindaqt_add_font_discovery_test(
    qindaqt_font_discovery_bounds_tests
    tst_font_discovery_bounds.cpp
    qindaqt.font-discovery-bounds
)

add_test(
    NAME qindaqt.font-discovery-boundary
    COMMAND
        "${CMAKE_COMMAND}"
        "-DSOURCE_ROOT=${CMAKE_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/check_boundary.cmake"
)
set_tests_properties(qindaqt.font-discovery-boundary PROPERTIES LABELS "service;fonts;policy")

add_test(
    NAME qindaqt.font-discovery-installed-consumer
    COMMAND
        "${CMAKE_COMMAND}"
        "-DQINDAQT_CMAKE=${CMAKE_COMMAND}"
        "-DQINDAQT_BUILD_DIRECTORY=${CMAKE_BINARY_DIR}"
        "-DQINDAQT_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/installed-font-discovery-stage"
        "-DQINDAQT_FONT_PREFERENCES_SOURCE_DIRECTORY=${CMAKE_SOURCE_DIR}/src/services/font_preferences"
        "-DQINDAQT_FONT_DISCOVERY_SOURCE_DIRECTORY=${CMAKE_SOURCE_DIR}/src/services/font_discovery"
        "-DQINDAQT_INSTALL_INCLUDEDIR=${CMAKE_INSTALL_INCLUDEDIR}"
        "-DQINDAQT_FONT_PREFERENCES_LIBRARY=$<TARGET_FILE:qindaqt_font_preferences>"
        "-DQINDAQT_FONT_DISCOVERY_LIBRARY=$<TARGET_FILE:qindaqt_font_discovery>"
        "-DQINDAQT_QT6_DIR=${Qt6_DIR}"
        "-DQINDAQT_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
        "-DQINDAQT_CONSUMER_SOURCE_DIRECTORY=${CMAKE_CURRENT_SOURCE_DIR}/installed_consumer"
        "-DQINDAQT_CONSUMER_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/installed_consumer/installed_font_discovery_consumer.cpp"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_installed_font_discovery_consumer.cmake"
)
set_tests_properties(qindaqt.font-discovery-installed-consumer PROPERTIES LABELS "service;fonts;installed")

# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_executable(qindaqt_font_catalog_tests tst_font_catalog.cpp)
target_link_libraries(
    qindaqt_font_catalog_tests
    PRIVATE QindaQt::FontPreferences Qt6::Test
)
qindaqt_enable_warnings(qindaqt_font_catalog_tests)
add_test(NAME qindaqt.font-catalog COMMAND qindaqt_font_catalog_tests)
set_tests_properties(qindaqt.font-catalog PROPERTIES LABELS "service;fonts;pure")

qt_add_executable(qindaqt_font_preferences_tests tst_font_preferences.cpp)
target_link_libraries(
    qindaqt_font_preferences_tests
    PRIVATE QindaQt::FontPreferences Qt6::Test
)
qindaqt_enable_warnings(qindaqt_font_preferences_tests)
add_test(NAME qindaqt.font-preferences COMMAND qindaqt_font_preferences_tests)
set_tests_properties(qindaqt.font-preferences PROPERTIES LABELS "service;fonts;pure")

qt_add_executable(qindaqt_font_preferences_codec_tests tst_font_preferences_codec.cpp)
target_link_libraries(
    qindaqt_font_preferences_codec_tests
    PRIVATE QindaQt::FontPreferences Qt6::Test
)
qindaqt_enable_warnings(qindaqt_font_preferences_codec_tests)
add_test(NAME qindaqt.font-preferences-codec COMMAND qindaqt_font_preferences_codec_tests)
set_tests_properties(qindaqt.font-preferences-codec PROPERTIES LABELS "service;fonts;pure")

qt_add_executable(qindaqt_font_bootstrap_tests tst_font_bootstrap.cpp)
target_link_libraries(
    qindaqt_font_bootstrap_tests
    PRIVATE QindaQt::FontPreferences Qt6::Test
)
qindaqt_enable_warnings(qindaqt_font_bootstrap_tests)
add_test(NAME qindaqt.font-bootstrap COMMAND qindaqt_font_bootstrap_tests)
set_tests_properties(qindaqt.font-bootstrap PROPERTIES LABELS "service;fonts;pure")

qt_add_executable(qindaqt_font_preferences_coordinator_tests tst_font_preferences_coordinator.cpp)
target_link_libraries(
    qindaqt_font_preferences_coordinator_tests
    PRIVATE QindaQt::FontPreferences Qt6::Test
)
qindaqt_enable_warnings(qindaqt_font_preferences_coordinator_tests)
add_test(NAME qindaqt.font-preferences-coordinator COMMAND qindaqt_font_preferences_coordinator_tests)
set_tests_properties(qindaqt.font-preferences-coordinator PROPERTIES LABELS "service;fonts;pure")

add_test(
    NAME qindaqt.font-preferences-boundary
    COMMAND
        "${CMAKE_COMMAND}"
        "-DSOURCE_ROOT=${CMAKE_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/check_boundary.cmake"
)
set_tests_properties(qindaqt.font-preferences-boundary PROPERTIES LABELS "service;fonts;pure;policy")

add_test(
    NAME qindaqt.font-preferences-installed-consumer
    COMMAND
        "${CMAKE_COMMAND}"
        "-DQINDAQT_CMAKE=${CMAKE_COMMAND}"
        "-DQINDAQT_BUILD_DIRECTORY=${CMAKE_BINARY_DIR}"
        "-DQINDAQT_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/installed-font-preferences-stage"
        "-DQINDAQT_FONT_PREFERENCES_SOURCE_DIRECTORY=${CMAKE_SOURCE_DIR}/src/services/font_preferences"
        "-DQINDAQT_INSTALL_INCLUDEDIR=${CMAKE_INSTALL_INCLUDEDIR}"
        "-DQINDAQT_FONT_PREFERENCES_LIBRARY=$<TARGET_FILE:qindaqt_font_preferences>"
        "-DQINDAQT_QT6_DIR=${Qt6_DIR}"
        "-DQINDAQT_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
        "-DQINDAQT_CONSUMER_SOURCE_DIRECTORY=${CMAKE_CURRENT_SOURCE_DIR}/installed_consumer"
        "-DQINDAQT_CONSUMER_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/installed_consumer/installed_font_consumer.cpp"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_installed_font_preferences_consumer.cmake"
)
set_tests_properties(qindaqt.font-preferences-installed-consumer PROPERTIES LABELS "service;fonts;installed")

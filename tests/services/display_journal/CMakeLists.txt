# SPDX-License-Identifier: GPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Core Test WaylandClient)
find_package(Wayland REQUIRED COMPONENTS Client)

qt_add_executable(
    qindaqt_display_journal_tests
    tst_file_journal_store.cpp
)
target_link_libraries(
    qindaqt_display_journal_tests
    PRIVATE
        QindaQt::DisplayJournal
        QindaQt::DisplayTopology
        Qt6::Core
        Qt6::Test
)
target_include_directories(
    qindaqt_display_journal_tests
    PRIVATE "${CMAKE_SOURCE_DIR}/src/services/display_journal/src"
)
qindaqt_enable_warnings(qindaqt_display_journal_tests)
add_test(NAME qindaqt.display-journal-filesystem COMMAND qindaqt_display_journal_tests)
set_tests_properties(
    qindaqt.display-journal-filesystem
    PROPERTIES LABELS "service;display;journal;hostile" TIMEOUT 20
)

add_test(
    NAME qindaqt.display-journal-installed-boundary-poison
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
set_tests_properties(
    qindaqt.display-journal-installed-boundary-poison
    PROPERTIES LABELS "package;service;display;journal;policy;hostile"
               RUN_SERIAL TRUE TIMEOUT 60
)

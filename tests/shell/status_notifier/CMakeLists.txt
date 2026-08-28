# SPDX-License-Identifier: GPL-3.0-or-later

# The shared build processes src/ before tests/, so the module target must
# exist by now. A missing target is a wiring/order defect and must fail the
# configure instead of silently producing an empty test selection.
if(NOT TARGET QindaQt::StatusNotifier)
    message(FATAL_ERROR
        "QindaQt::StatusNotifier is missing; add_subdirectory(src/shell/status_notifier)"
        " must run before tests/shell/status_notifier")
endif()

find_package(Qt6 6.11 REQUIRED COMPONENTS Test)

qt_add_executable(qindaqt_status_notifier_values_tests tst_status_notifier_values.cpp)
target_link_libraries(qindaqt_status_notifier_values_tests
    PRIVATE QindaQt::StatusNotifier Qt6::Core Qt6::Test)
qindaqt_enable_warnings(qindaqt_status_notifier_values_tests)
add_test(NAME qindaqt.status-notifier-values COMMAND qindaqt_status_notifier_values_tests)
set_tests_properties(qindaqt.status-notifier-values PROPERTIES LABELS "shell;status-notifier")

qt_add_executable(qindaqt_status_notifier_registry_tests tst_status_notifier_registry.cpp)
target_link_libraries(qindaqt_status_notifier_registry_tests
    PRIVATE QindaQt::StatusNotifier Qt6::Core Qt6::Test)
qindaqt_enable_warnings(qindaqt_status_notifier_registry_tests)
add_test(NAME qindaqt.status-notifier-registry COMMAND qindaqt_status_notifier_registry_tests)
set_tests_properties(qindaqt.status-notifier-registry PROPERTIES LABELS "shell;status-notifier")

qt_add_executable(qindaqt_status_notifier_presentation_tests tst_status_notifier_presentation.cpp)
target_link_libraries(qindaqt_status_notifier_presentation_tests
    PRIVATE QindaQt::StatusNotifier Qt6::Core Qt6::Test)
qindaqt_enable_warnings(qindaqt_status_notifier_presentation_tests)
add_test(NAME qindaqt.status-notifier-presentation COMMAND qindaqt_status_notifier_presentation_tests)
set_tests_properties(qindaqt.status-notifier-presentation PROPERTIES LABELS "shell;status-notifier")

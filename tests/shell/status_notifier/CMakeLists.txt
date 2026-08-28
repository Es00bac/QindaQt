# SPDX-License-Identifier: GPL-3.0-or-later

# The status-notifier S0 module is wired into the shared build by the
# integration branch. Until that single add_subdirectory() lands, skip these
# tests instead of failing configuration.
if(NOT TARGET QindaQt::StatusNotifier)
    return()
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

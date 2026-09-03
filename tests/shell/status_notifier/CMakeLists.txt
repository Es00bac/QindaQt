# SPDX-License-Identifier: GPL-3.0-or-later

# The shared build processes src/ before tests/, so the module target must
# exist by now. A missing target is a wiring/order defect and must fail the
# configure instead of silently producing an empty test selection.
if(NOT TARGET QindaQt::StatusNotifier)
    message(FATAL_ERROR
        "QindaQt::StatusNotifier is missing; add_subdirectory(src/shell/status_notifier)"
        " must run before tests/shell/status_notifier")
endif()

find_package(Qt6 6.11 REQUIRED COMPONENTS DBus Gui Test)

qt_add_executable(qindaqt_status_notifier_values_tests tst_status_notifier_values.cpp)
target_compile_definitions(qindaqt_status_notifier_values_tests
    PRIVATE QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
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

if(NOT TARGET QindaQt::StatusNotifierWatcher)
    message(FATAL_ERROR
        "QindaQt::StatusNotifierWatcher is missing; the watcher transport library"
        " must build before the status-notifier transport tests")
endif()
if(NOT TARGET QindaQt::StatusNotifierItemClient)
    message(FATAL_ERROR
        "QindaQt::StatusNotifierItemClient is missing; the item-client transport"
        " library must build before the status-notifier transport tests")
endif()
if(NOT TARGET QindaQt::StatusNotifierIcon)
    message(FATAL_ERROR
        "QindaQt::StatusNotifierIcon is missing; the icon library must build"
        " before the status-notifier icon tests")
endif()

qt_add_executable(qindaqt_status_notifier_watcher_tests tst_status_notifier_watcher.cpp)
target_link_libraries(qindaqt_status_notifier_watcher_tests
    PRIVATE QindaQt::StatusNotifierWatcher Qt6::Core Qt6::DBus Qt6::Test)
qindaqt_enable_warnings(qindaqt_status_notifier_watcher_tests)
add_test(NAME qindaqt.status-notifier-watcher COMMAND qindaqt_status_notifier_watcher_tests)
set_tests_properties(qindaqt.status-notifier-watcher PROPERTIES LABELS "shell;status-notifier")

qt_add_executable(qindaqt_status_notifier_item_client_tests
    tst_status_notifier_item_client.cpp
    status_notifier_fake_item_test_support.h
    status_notifier_private_bus_test_support.h)
target_link_libraries(qindaqt_status_notifier_item_client_tests
    PRIVATE QindaQt::StatusNotifierItemClient Qt6::Core Qt6::DBus Qt6::Test)
qindaqt_enable_warnings(qindaqt_status_notifier_item_client_tests)
add_test(NAME qindaqt.status-notifier-item-client COMMAND qindaqt_status_notifier_item_client_tests)
set_tests_properties(qindaqt.status-notifier-item-client PROPERTIES LABELS "shell;status-notifier")

qt_add_executable(qindaqt_status_notifier_monitor_tests
    tst_status_notifier_monitor.cpp
    status_notifier_fake_item_test_support.h
    status_notifier_private_bus_test_support.h)
target_link_libraries(qindaqt_status_notifier_monitor_tests
    PRIVATE
        QindaQt::StatusNotifierItemClient
        QindaQt::StatusNotifierWatcher
        Qt6::Core
        Qt6::DBus
        Qt6::Test)
qindaqt_enable_warnings(qindaqt_status_notifier_monitor_tests)
add_test(NAME qindaqt.status-notifier-monitor COMMAND qindaqt_status_notifier_monitor_tests)
set_tests_properties(qindaqt.status-notifier-monitor PROPERTIES LABELS "shell;status-notifier")

qt_add_executable(qindaqt_status_notifier_icon_tests tst_status_notifier_icon.cpp)
target_link_libraries(qindaqt_status_notifier_icon_tests
    PRIVATE QindaQt::StatusNotifierIcon Qt6::Core Qt6::Gui Qt6::Test)
qindaqt_enable_warnings(qindaqt_status_notifier_icon_tests)
add_test(NAME qindaqt.status-notifier-icon COMMAND qindaqt_status_notifier_icon_tests)
set_tests_properties(qindaqt.status-notifier-icon PROPERTIES LABELS "shell;status-notifier")

add_subdirectory(applet)

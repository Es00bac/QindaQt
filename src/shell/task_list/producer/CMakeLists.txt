# SPDX-License-Identifier: LGPL-3.0-or-later

# AGENT-NOTE: The producer is deliberately not instantiated by the production
# shell yet; shell composition (a later lane) injects the bus connection and
# the TaskListSource. See docs/wiki/shell/task-list.md.

find_package(Qt6 6.11 REQUIRED COMPONENTS Core DBus)

qt_add_library(qindaqt_shell_task_list_producer STATIC)
add_library(QindaQt::ShellTaskListProducer ALIAS qindaqt_shell_task_list_producer)

target_sources(
    qindaqt_shell_task_list_producer
    PRIVATE
        src/task_list_fact_joiner.cpp
        src/task_list_facts_producer.cpp
        src/task_list_wire.cpp
        src/qt_task_list_producer_transport.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/shell/task_list/producer/qt_task_list_producer_transport.h
            include/qindaqt/shell/task_list/producer/task_list_fact_joiner.h
            include/qindaqt/shell/task_list/producer/task_list_facts_producer.h
            include/qindaqt/shell/task_list/producer/task_list_producer_transport.h
            include/qindaqt/shell/task_list/producer/task_list_wire.h
)
target_link_libraries(
    qindaqt_shell_task_list_producer
    PUBLIC QindaQt::ShellTaskList Qt6::Core Qt6::DBus
    PRIVATE QindaQt::ShellVisibilityProtocol
)
target_compile_features(qindaqt_shell_task_list_producer PUBLIC cxx_std_20)
set_target_properties(
    qindaqt_shell_task_list_producer
    PROPERTIES CXX_EXTENSIONS OFF AUTOMOC_PATH_PREFIX ON
)
qindaqt_enable_warnings(qindaqt_shell_task_list_producer)

install(
    TARGETS qindaqt_shell_task_list_producer
    EXPORT QindaQtTargets
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

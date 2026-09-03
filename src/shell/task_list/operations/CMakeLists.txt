# SPDX-License-Identifier: LGPL-3.0-or-later

# AGENT-NOTE: The operation adapter is deliberately not instantiated by the
# production shell yet; shell composition (a later lane) injects the producer,
# transport, and intent source. See docs/wiki/shell/task-list.md.

find_package(Qt6 6.11 REQUIRED COMPONENTS Core DBus)

qt_add_library(qindaqt_shell_task_list_operations STATIC)
add_library(
    QindaQt::ShellTaskListOperations ALIAS qindaqt_shell_task_list_operations
)

target_sources(
    qindaqt_shell_task_list_operations
    PRIVATE
        src/task_list_operation_adapter.cpp
        src/qt_task_list_operation_transport.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/shell/task_list/operations/qt_task_list_operation_transport.h
            include/qindaqt/shell/task_list/operations/task_list_operation_adapter.h
            include/qindaqt/shell/task_list/operations/task_list_operation_transport.h
            include/qindaqt/shell/task_list/operations/task_list_operations.h
)
target_link_libraries(
    qindaqt_shell_task_list_operations
    PUBLIC QindaQt::ShellTaskListProducer Qt6::Core Qt6::DBus
)
target_compile_features(qindaqt_shell_task_list_operations PUBLIC cxx_std_20)
set_target_properties(
    qindaqt_shell_task_list_operations
    PROPERTIES CXX_EXTENSIONS OFF AUTOMOC_PATH_PREFIX ON
)
qindaqt_enable_warnings(qindaqt_shell_task_list_operations)

install(
    TARGETS qindaqt_shell_task_list_operations
    EXPORT QindaQtTargets
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

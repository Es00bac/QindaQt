# SPDX-License-Identifier: LGPL-3.0-or-later

# AGENT-NOTE: This module is intentionally not wired into src/shell/CMakeLists.txt
# yet; the task-list T0 lane is source/static only. The integrating manager adds
# the single add_subdirectory() line in the parent build when wiring this slice.

find_package(Qt6 6.11 REQUIRED COMPONENTS Core)

qt_add_library(qindaqt_shell_task_list STATIC)
add_library(QindaQt::ShellTaskList ALIAS qindaqt_shell_task_list)

target_sources(
    qindaqt_shell_task_list
    PRIVATE
        src/task_list_filter.cpp
        src/task_list_grouping.cpp
        src/task_list_presentation.cpp
        src/task_list_source.cpp
        src/task_list_validation.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/shell/task_list/task_list_filter.h
            include/qindaqt/shell/task_list/task_list_intent.h
            include/qindaqt/shell/task_list/task_list_presentation.h
            include/qindaqt/shell/task_list/task_list_source.h
            include/qindaqt/shell/task_list/task_list_types.h
)
target_link_libraries(qindaqt_shell_task_list PUBLIC Qt6::Core)
target_compile_features(qindaqt_shell_task_list PUBLIC cxx_std_20)
set_target_properties(qindaqt_shell_task_list PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_shell_task_list)

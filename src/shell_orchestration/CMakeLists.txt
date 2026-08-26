# SPDX-License-Identifier: LGPL-3.0-or-later

qt_add_library(qindaqt_shell_orchestration STATIC)
add_library(QindaQt::ShellOrchestration ALIAS qindaqt_shell_orchestration)

target_sources(
    qindaqt_shell_orchestration
    PRIVATE
        src/output_inventory_matcher.cpp
        src/panel_interaction_store.cpp
        src/panel_runtime_plan_assembler.cpp
        src/panel_visibility_inventory_assembler.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/shell_orchestration/output_inventory_matcher.h
            include/qindaqt/shell_orchestration/panel_interaction_store.h
            include/qindaqt/shell_orchestration/panel_runtime_plan_assembler.h
            include/qindaqt/shell_orchestration/panel_visibility_inventory_assembler.h
)
target_link_libraries(
    qindaqt_shell_orchestration
    PUBLIC
        QindaQt::ShellLayout
        QindaQt::ShellSurface
        QindaQt::ShellVisibility
        Qt6::Core
)
target_compile_features(qindaqt_shell_orchestration PUBLIC cxx_std_20)
set_target_properties(qindaqt_shell_orchestration PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_shell_orchestration)

install(
    TARGETS qindaqt_shell_orchestration
    EXPORT QindaQtTargets
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

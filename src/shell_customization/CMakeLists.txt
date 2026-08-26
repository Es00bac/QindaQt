# SPDX-License-Identifier: LGPL-3.0-or-later

qt_add_library(qindaqt_shell_customization STATIC)
add_library(QindaQt::ShellCustomization ALIAS qindaqt_shell_customization)

target_sources(
    qindaqt_shell_customization
    PRIVATE
        src/applet_edit_mutation.cpp
        src/applet_placement_validator.cpp
        src/editing_commands.cpp
        src/layout_candidate_validator.cpp
        src/layout_edit_helpers.cpp
        src/layout_edit_mutation.cpp
        src/layout_edit_preparation.cpp
        src/layout_edit_request.cpp
        src/layout_editing_coordinator.cpp
        src/layout_editing_evaluation.cpp
        src/layout_editing_repository.cpp
        src/panel_edit_mutation.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/shell_customization/editing_commands.h
            include/qindaqt/shell_customization/editing_result.h
            include/qindaqt/shell_customization/layout_editing_coordinator.h
            include/qindaqt/shell_customization/layout_editing_repository.h
)

target_link_libraries(
    qindaqt_shell_customization
    PUBLIC
        qindaqt_applets
        qindaqt_profiles
        QindaQt::ShellLayout
        Qt6::Core
)
target_compile_features(qindaqt_shell_customization PUBLIC cxx_std_20)
set_target_properties(qindaqt_shell_customization PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_shell_customization)

install(
    TARGETS qindaqt_shell_customization
    EXPORT QindaQtTargets
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

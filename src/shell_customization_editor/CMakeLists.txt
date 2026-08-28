# SPDX-License-Identifier: LGPL-3.0-or-later

qt_add_library(qindaqt_shell_customization_editor STATIC)
add_library(QindaQt::ShellCustomizationEditor ALIAS qindaqt_shell_customization_editor)

target_sources(
    qindaqt_shell_customization_editor
    PRIVATE
        src/accessibility_identity.cpp
        src/coordinator_engine_adapter.cpp
        src/editor_intent.cpp
        src/editor_session.cpp
        src/editor_session_gestures.cpp
        src/gesture_state_machine.cpp
        src/intent_translator.cpp
        src/keyboard_navigation.cpp
        src/user_profile_store.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/shell_customization_editor/accessibility_identity.h
            include/qindaqt/shell_customization_editor/coordinator_engine_adapter.h
            include/qindaqt/shell_customization_editor/editing_engine.h
            include/qindaqt/shell_customization_editor/editor_intent.h
            include/qindaqt/shell_customization_editor/editor_session.h
            include/qindaqt/shell_customization_editor/gesture_state_machine.h
            include/qindaqt/shell_customization_editor/intent_translator.h
            include/qindaqt/shell_customization_editor/keyboard_navigation.h
            include/qindaqt/shell_customization_editor/user_profile_store.h
)

target_link_libraries(
    qindaqt_shell_customization_editor
    PUBLIC
        QindaQt::ShellCustomization
        Qt6::Core
)
target_compile_features(qindaqt_shell_customization_editor PUBLIC cxx_std_20)
set_target_properties(qindaqt_shell_customization_editor PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_shell_customization_editor)

install(
    TARGETS qindaqt_shell_customization_editor
    EXPORT QindaQtTargets
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

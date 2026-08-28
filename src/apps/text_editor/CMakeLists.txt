# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_library(qindaqt_editor_support STATIC)
target_sources(
    qindaqt_editor_support
    PRIVATE
        document/document_controller.cpp
        document/document_state.cpp
        document/local_document_store.cpp
        ui/editor_appearance.cpp
        ui/editor_window.cpp
    PUBLIC
        document/document_controller.h
        document/document_state.h
        document/document_store.h
        document/document_types.h
        document/local_document_store.h
        ui/editor_appearance.h
        ui/editor_window.h
)
target_include_directories(qindaqt_editor_support PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(
    qindaqt_editor_support
    PUBLIC QindaQt::DesignTokens qindaqt_themes Qt6::Core Qt6::Gui Qt6::Widgets
)
target_compile_features(qindaqt_editor_support PUBLIC cxx_std_20)
set_target_properties(qindaqt_editor_support PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_editor_support)

qt_add_executable(qindaqt-editor main.cpp)
target_link_libraries(qindaqt-editor PRIVATE qindaqt_editor_support)
target_compile_features(qindaqt-editor PRIVATE cxx_std_20)
set_target_properties(qindaqt-editor PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt-editor)

install(
    TARGETS qindaqt-editor
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    COMPONENT TextEditor
)
install(
    FILES org.qindaqt.TextEditor.desktop
    DESTINATION "${CMAKE_INSTALL_DATADIR}/applications"
    COMPONENT TextEditor
)
install(
    DIRECTORY "${PROJECT_SOURCE_DIR}/data/themes/"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/qindaqt/themes"
    COMPONENT TextEditor
)

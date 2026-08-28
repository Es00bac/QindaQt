# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_library(qindaqt_editor_support STATIC)
target_sources(
    qindaqt_editor_support
    PRIVATE
        app_shell/editor_action_catalog.cpp
        app_shell/editor_app_shell_bridge.cpp
        app_shell/fail_closed_file_selection_adapter.cpp
        app_shell/native_file_selection_adapter.cpp
        document/document_controller.cpp
        document/document_state.cpp
        document/local_document_store.cpp
        ui/editor_appearance.cpp
        ui/editor_window.cpp
    PUBLIC
        app_shell/editor_action_catalog.h
        app_shell/editor_app_shell_bridge.h
        app_shell/fail_closed_file_selection_adapter.h
        app_shell/file_selection_adapter.h
        app_shell/native_file_selection_adapter.h
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
    PUBLIC
        QindaQt::AppShell
        QindaQt::DesignTokens
        qindaqt_themes
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
)
target_compile_features(qindaqt_editor_support PUBLIC cxx_std_20)
set_target_properties(qindaqt_editor_support PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_editor_support)

qt_add_executable(qindaqt-editor main.cpp)
target_link_libraries(qindaqt-editor PRIVATE qindaqt_editor_support)
target_compile_features(qindaqt-editor PRIVATE cxx_std_20)

# AGENT-CONTRACT: qindaqt_app_shell is QindaQt::AppShell's SHARED backing
# library (src/app_shell/CMakeLists.txt), installed beside the QML module
# tree rather than CMAKE_INSTALL_BINDIR. Without this relative RPATH the
# installed editor cannot resolve libqindaqt_app_shell.so even though the
# build tree links successfully.
set(qindaqt_editor_install_bindir "${CMAKE_INSTALL_BINDIR}")
cmake_path(
    ABSOLUTE_PATH qindaqt_editor_install_bindir
    BASE_DIRECTORY "${CMAKE_INSTALL_PREFIX}"
    NORMALIZE
    OUTPUT_VARIABLE qindaqt_editor_install_bindir_absolute
)
set(qindaqt_editor_install_qmldir "${QT6_INSTALL_QML}")
cmake_path(
    ABSOLUTE_PATH qindaqt_editor_install_qmldir
    BASE_DIRECTORY "${CMAKE_INSTALL_PREFIX}"
    NORMALIZE
    OUTPUT_VARIABLE qindaqt_editor_install_qmldir_absolute
)
cmake_path(
    RELATIVE_PATH qindaqt_editor_install_qmldir_absolute
    BASE_DIRECTORY "${qindaqt_editor_install_bindir_absolute}"
    OUTPUT_VARIABLE qindaqt_editor_qml_relative_path
)
set_target_properties(
    qindaqt-editor
    PROPERTIES
        CXX_EXTENSIONS OFF
        INSTALL_RPATH "$ORIGIN/${qindaqt_editor_qml_relative_path}/QindaQt/AppShell"
)
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

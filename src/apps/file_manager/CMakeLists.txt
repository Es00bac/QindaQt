# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_library(qindaqt_file_manager_support STATIC)
target_sources(
    qindaqt_file_manager_support
    PRIVATE
        model/launch_intent.cpp
        model/local_directory_lister.cpp
        model/navigation_controller.cpp
        model/navigation_history.cpp
    PUBLIC
        model/directory_lister.h
        model/file_manager_types.h
        model/launch_intent.h
        model/local_directory_lister.h
        model/navigation_controller.h
        model/navigation_history.h
)
target_include_directories(qindaqt_file_manager_support PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(qindaqt_file_manager_support PUBLIC Qt6::Core Qt6::Gui)
target_compile_features(qindaqt_file_manager_support PUBLIC cxx_std_20)
set_target_properties(qindaqt_file_manager_support PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_file_manager_support)

qt_add_executable(qindaqt-file-manager main.cpp)
qt_add_qml_module(
    qindaqt-file-manager
    URI QindaQt.FileManagerApp
    VERSION 1.0
    QML_FILES
        ui/Main.qml
        ui/Toolbar.qml
        ui/Breadcrumb.qml
        ui/EntryList.qml
        ui/StatePane.qml
)
# AGENT-NOTE: qindaqt-file-manager is a plain runtime QML consumer of
# QindaQt.Controls, not a link-time dependency: Controls resolves through Qt's
# QML plugin/import path, matching the pattern in
# tests/controls/control_test_support.cpp. The explicit build-order
# dependency still guarantees both sibling qmlplugins exist before this
# target's own qmlcachegen/qmllint step runs.
target_compile_definitions(
    qindaqt-file-manager
    PRIVATE QINDAQT_QML_IMPORT_PATH="${CMAKE_BINARY_DIR}/qml"
)
target_link_libraries(
    qindaqt-file-manager
    PRIVATE
        qindaqt_file_manager_support
        QindaQt::DesignTokens
        QindaQt::TokensQml
        qindaqt_themes
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
)
add_dependencies(qindaqt-file-manager qindaqt_tokens_qmlplugin qindaqt_controls_qmlplugin)
target_compile_features(qindaqt-file-manager PRIVATE cxx_std_20)
set_target_properties(qindaqt-file-manager PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt-file-manager)

install(
    TARGETS qindaqt-file-manager
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    COMPONENT FileManager
)
install(
    FILES org.qindaqt.FileManager.desktop
    DESTINATION "${CMAKE_INSTALL_DATADIR}/applications"
    COMPONENT FileManager
)
install(
    DIRECTORY "${PROJECT_SOURCE_DIR}/data/themes/"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/qindaqt/themes"
    COMPONENT FileManager
)

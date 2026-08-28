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

qt_add_executable(qindaqt-file-manager
    main.cpp
    runtime/qml_component_ready.cpp
    runtime/qml_component_ready.h
)
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

set(qindaqt_file_manager_install_bindir "${CMAKE_INSTALL_BINDIR}")
cmake_path(
    ABSOLUTE_PATH qindaqt_file_manager_install_bindir
    BASE_DIRECTORY "${CMAKE_INSTALL_PREFIX}"
    NORMALIZE
    OUTPUT_VARIABLE qindaqt_file_manager_install_bindir_absolute
)
set(qindaqt_file_manager_install_qmldir "${QT6_INSTALL_QML}")
cmake_path(
    ABSOLUTE_PATH qindaqt_file_manager_install_qmldir
    BASE_DIRECTORY "${CMAKE_INSTALL_PREFIX}"
    NORMALIZE
    OUTPUT_VARIABLE qindaqt_file_manager_install_qmldir_absolute
)
cmake_path(
    RELATIVE_PATH qindaqt_file_manager_install_qmldir_absolute
    BASE_DIRECTORY "${qindaqt_file_manager_install_bindir_absolute}"
    OUTPUT_VARIABLE qindaqt_file_manager_qml_relative_path
)

# AGENT-CONTRACT: Production import and loader discovery must remain relative
# to the executable's installed location. Embedding ${CMAKE_BINARY_DIR}/qml
# lets a staged test pass by importing a developer tree that a packaged user
# does not have; see docs/wiki/apps/file-manager.md.
target_compile_definitions(
    qindaqt-file-manager
    PRIVATE
        QINDAQT_INSTALL_QML_RELATIVE_PATH="${qindaqt_file_manager_qml_relative_path}"
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
set_target_properties(
    qindaqt-file-manager
    PROPERTIES
        CXX_EXTENSIONS OFF
        INSTALL_RPATH
            "$ORIGIN/${qindaqt_file_manager_qml_relative_path}/QindaQt/Tokens"
)
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

# FileManager is a usable package component, not merely an executable payload.
# Repeat its two public QML runtime dependencies into the same component so
# component-only staging and downstream packages cannot omit a backing library,
# plugin, qmldir, or Controls source named by qmldir.
qt_query_qml_module(
    qindaqt_tokens_qml
    QMLDIR qindaqt_file_manager_tokens_qmldir
    TYPEINFO qindaqt_file_manager_tokens_typeinfo
)
qt_query_qml_module(
    qindaqt_controls_qml
    QMLDIR qindaqt_file_manager_controls_qmldir
    TYPEINFO qindaqt_file_manager_controls_typeinfo
    QML_FILES qindaqt_file_manager_controls_qml_files
    QML_FILES_DEPLOY_PATHS qindaqt_file_manager_controls_deploy_paths
)
install(
    TARGETS
        qindaqt_tokens_qml
        qindaqt_tokens_qmlplugin
    RUNTIME DESTINATION "${QT6_INSTALL_QML}/QindaQt/Tokens" COMPONENT FileManager
    LIBRARY DESTINATION "${QT6_INSTALL_QML}/QindaQt/Tokens" COMPONENT FileManager
    ARCHIVE DESTINATION "${QT6_INSTALL_QML}/QindaQt/Tokens" COMPONENT FileManager
)
install(
    FILES
        "${qindaqt_file_manager_tokens_qmldir}"
        "${qindaqt_file_manager_tokens_typeinfo}"
    DESTINATION "${QT6_INSTALL_QML}/QindaQt/Tokens"
    COMPONENT FileManager
)

install(
    TARGETS
        qindaqt_controls_qml
        qindaqt_controls_qmlplugin
    RUNTIME DESTINATION "${QT6_INSTALL_QML}/QindaQt/Controls" COMPONENT FileManager
    LIBRARY DESTINATION "${QT6_INSTALL_QML}/QindaQt/Controls" COMPONENT FileManager
    ARCHIVE DESTINATION "${QT6_INSTALL_QML}/QindaQt/Controls" COMPONENT FileManager
)
install(
    FILES
        "${qindaqt_file_manager_controls_qmldir}"
        "${qindaqt_file_manager_controls_typeinfo}"
    DESTINATION "${QT6_INSTALL_QML}/QindaQt/Controls"
    COMPONENT FileManager
)
foreach(qml_file deploy_path IN ZIP_LISTS
        qindaqt_file_manager_controls_qml_files
        qindaqt_file_manager_controls_deploy_paths)
    cmake_path(GET deploy_path PARENT_PATH deploy_directory)
    cmake_path(GET deploy_path FILENAME deploy_name)
    install(
        FILES "${qml_file}"
        DESTINATION "${QT6_INSTALL_QML}/QindaQt/Controls/${deploy_directory}"
        RENAME "${deploy_name}"
        COMPONENT FileManager
    )
endforeach()

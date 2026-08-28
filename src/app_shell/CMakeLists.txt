# SPDX-License-Identifier: LGPL-3.0-or-later

set(QINDAQT_APP_SHELL_QML_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/qml/QindaQt/AppShell")

qt_add_library(qindaqt_app_shell SHARED)
add_library(QindaQt::AppShell ALIAS qindaqt_app_shell)

target_sources(
    qindaqt_app_shell
    PRIVATE
        src/action_registry.cpp
        src/app_shell_types.cpp
        src/application_coordinator.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/app_shell/action_registry.h
            include/qindaqt/app_shell/app_shell_types.h
            include/qindaqt/app_shell/application_coordinator.h
)
qt_add_qml_module(
    qindaqt_app_shell
    URI QindaQt.AppShell
    VERSION 1.0
    OUTPUT_DIRECTORY "${QINDAQT_APP_SHELL_QML_OUTPUT_DIRECTORY}"
    TYPEINFO qindaqt_app_shell.qmltypes
    # Keep the QML registration header in Qt's module source inventory. The
    # generated registrar must include the real public declaration rather than
    # compile against an undeclared C++ type.
    SOURCES include/qindaqt/app_shell/application_coordinator.h
    QML_FILES qml/ApplicationShell.qml
)

target_include_directories(
    qindaqt_app_shell
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include/qindaqt/app_shell"
)

qt_query_qml_module(
    qindaqt_app_shell
    QML_FILES qindaqt_app_shell_qml_files
    QML_FILES_DEPLOY_PATHS qindaqt_app_shell_deploy_paths
)
list(LENGTH qindaqt_app_shell_qml_files qindaqt_app_shell_qml_file_count)
list(LENGTH qindaqt_app_shell_deploy_paths qindaqt_app_shell_deploy_path_count)
if(NOT qindaqt_app_shell_qml_file_count EQUAL qindaqt_app_shell_deploy_path_count)
    message(FATAL_ERROR "AppShell QML sources and Qt deploy paths must remain one-to-one")
endif()
set_property(
    TARGET qindaqt_app_shell
    PROPERTY QINDAQT_QML_DEPLOY_PATHS "${qindaqt_app_shell_deploy_paths}"
)

foreach(qml_file deploy_path IN ZIP_LISTS
        qindaqt_app_shell_qml_files qindaqt_app_shell_deploy_paths)
    cmake_path(GET deploy_path PARENT_PATH deploy_directory)
    cmake_path(GET deploy_path FILENAME deploy_name)
    # AGENT-CONTRACT: Tooling and runtime consumers receive the exact same
    # deploy paths generated into qmldir; do not hand-maintain a second list.
    install(
        FILES "${qml_file}"
        DESTINATION "${QT6_INSTALL_QML}/QindaQt/AppShell/${deploy_directory}"
        RENAME "${deploy_name}"
    )
endforeach()

target_link_libraries(
    qindaqt_app_shell
    PUBLIC Qt6::Core Qt6::Gui Qt6::Qml
    PRIVATE QindaQt::TokensQml QindaQt::ControlsQml Qt6::Quick Qt6::QuickControls2
)
target_compile_features(qindaqt_app_shell PUBLIC cxx_std_20)
set_target_properties(
    qindaqt_app_shell
    PROPERTIES
        CXX_EXTENSIONS OFF
        INSTALL_RPATH "$ORIGIN/../Tokens;$ORIGIN/../Controls"
)
qindaqt_enable_warnings(qindaqt_app_shell)
add_dependencies(
    qindaqt_app_shell
    qindaqt_tokens_qmlplugin
    qindaqt_controls_qmlplugin
)

# The backing library contains both the public C++ contract and compiled QML.
# It must remain beside its generated plugin and qmldir for relocatable imports.
install(
    TARGETS qindaqt_app_shell qindaqt_app_shellplugin
    EXPORT QindaQtTargets
    RUNTIME DESTINATION "${QT6_INSTALL_QML}/QindaQt/AppShell"
    LIBRARY DESTINATION "${QT6_INSTALL_QML}/QindaQt/AppShell"
    ARCHIVE DESTINATION "${QT6_INSTALL_QML}/QindaQt/AppShell"
    FILE_SET HEADERS DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
install(
    FILES
        "${QINDAQT_APP_SHELL_QML_OUTPUT_DIRECTORY}/qmldir"
        "${QINDAQT_APP_SHELL_QML_OUTPUT_DIRECTORY}/qindaqt_app_shell.qmltypes"
    DESTINATION "${QT6_INSTALL_QML}/QindaQt/AppShell"
)

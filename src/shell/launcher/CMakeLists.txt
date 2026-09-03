# SPDX-License-Identifier: LGPL-3.0-or-later

# Launcher L0 is wired through the repository root and may also be configured
# through the focused standalone test route.
qt_add_library(qindaqt_shell_launcher STATIC)
add_library(QindaQt::ShellLauncher ALIAS qindaqt_shell_launcher)

target_sources(
    qindaqt_shell_launcher
    PRIVATE
        src/application_catalog.cpp
        src/desktop_entry_parser.cpp
        src/launcher_category_model.cpp
        src/launcher_pinned_recent.cpp
        src/launcher_presentation.cpp
        src/launcher_search_ranker.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/shell_launcher/application_catalog.h
            include/qindaqt/shell_launcher/desktop_entry_parser.h
            include/qindaqt/shell_launcher/launcher_bounds.h
            include/qindaqt/shell_launcher/launcher_category_model.h
            include/qindaqt/shell_launcher/launcher_pinned_recent.h
            include/qindaqt/shell_launcher/launcher_presentation.h
            include/qindaqt/shell_launcher/launcher_search_ranker.h
            include/qindaqt/shell_launcher/launcher_types.h
)

# AGENT-CONTRACT: Qt Core only. The launcher model must never link Qt Gui,
# Qt Quick, Qt DBus, or KDE frameworks; platform adaptation (entry scanning,
# process execution, persistence) lives behind later boundaries owned by
# other modules (see docs/wiki/adr/0042-launcher-model-without-execution.md).
target_link_libraries(qindaqt_shell_launcher PUBLIC Qt6::Core)
target_compile_features(qindaqt_shell_launcher PUBLIC cxx_std_20)
set_target_properties(qindaqt_shell_launcher PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_shell_launcher)

install(
    TARGETS qindaqt_shell_launcher
    EXPORT QindaQtTargets
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

# Launcher L1 production adapters and the compiled QindaQt.Shell.Launcher
# applet module. The definition is deferred to the end of the project scope
# because this directory is processed before its dependencies (controls,
# tokens, settings client) in the repository build; a focused standalone
# route that provides the same targets works unchanged. A pure-only route
# without those targets simply skips the runtime.
function(qindaqt_add_launcher_runtime)
    get_property(module_dir GLOBAL PROPERTY QINDAQT_LAUNCHER_MODULE_DIR)
    get_property(repo_root GLOBAL PROPERTY QINDAQT_LAUNCHER_REPO_ROOT)
    if(TARGET qindaqt_shell_launcher_runtime)
        return()
    endif()
    if(NOT TARGET qindaqt_settings_client
       OR NOT TARGET qindaqt_tokens_qml OR NOT TARGET qindaqt_controls_qml)
        message(STATUS
            "Launcher runtime dependencies unavailable; building the pure L0 model only")
        return()
    endif()

    find_package(Qt6 6.11 REQUIRED COMPONENTS DBus Gui Qml Quick QuickControls2)

    qt_add_library(qindaqt_shell_launcher_runtime STATIC)
    add_library(QindaQt::ShellLauncherRuntime ALIAS qindaqt_shell_launcher_runtime)
    target_sources(
        qindaqt_shell_launcher_runtime
        PRIVATE
            "${module_dir}/src/application_scanner.cpp"
            "${module_dir}/src/launch_activator.cpp"
            "${module_dir}/src/launch_execution.cpp"
            "${module_dir}/src/launch_executor.cpp"
            "${module_dir}/src/launch_spawner.cpp"
            "${module_dir}/src/launcher_applet_controller.cpp"
            "${module_dir}/src/launcher_persistence.cpp"
    )
    target_include_directories(
        qindaqt_shell_launcher_runtime
        PUBLIC "${module_dir}/src"
    )
    # AGENT-CONTRACT: The runtime target may use the filesystem (scanner),
    # process spawning (spawner), and the session bus (activator) only inside
    # its dedicated adapter files; the boundary tests enforce the split.
    target_link_libraries(
        qindaqt_shell_launcher_runtime
        PUBLIC
            QindaQt::ShellLauncher
            QindaQt::SettingsClient
            Qt6::Core
            Qt6::DBus
            Qt6::Qml
            Qt6::Quick
    )
    target_compile_features(qindaqt_shell_launcher_runtime PUBLIC cxx_std_20)
    set_target_properties(qindaqt_shell_launcher_runtime
                          PROPERTIES CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON)
    qindaqt_enable_warnings(qindaqt_shell_launcher_runtime)

    # The compiled applet module is pure presentation over the shell-private
    # controller; it carries no C++ types of its own.
    set(QINDAQT_LAUNCHER_QML_OUTPUT_DIRECTORY
        "${CMAKE_BINARY_DIR}/qml/QindaQt/Shell/Launcher")
    qt_add_library(qindaqt_shell_launcher_qml SHARED)
    add_library(QindaQt::ShellLauncherQml ALIAS qindaqt_shell_launcher_qml)
    # Deferred scope differs from the module directory; per-file aliases keep
    # the Qt resource paths correct regardless of the caller's directory.
    set_source_files_properties("${module_dir}/qml/LauncherApplet.qml"
                                PROPERTIES QT_RESOURCE_ALIAS LauncherApplet.qml)
    set_source_files_properties("${module_dir}/qml/LauncherSection.qml"
                                PROPERTIES QT_RESOURCE_ALIAS LauncherSection.qml)
    qt_add_qml_module(
        qindaqt_shell_launcher_qml
        URI QindaQt.Shell.Launcher
        VERSION 1.0
        OUTPUT_DIRECTORY "${QINDAQT_LAUNCHER_QML_OUTPUT_DIRECTORY}"
        TYPEINFO qindaqt_shell_launcher.qmltypes
        QML_FILES
            "${module_dir}/qml/LauncherApplet.qml"
            "${module_dir}/qml/LauncherSection.qml"
    )
    target_link_libraries(
        qindaqt_shell_launcher_qml
        PRIVATE Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2
    )
    set_target_properties(qindaqt_shell_launcher_qml PROPERTIES CXX_EXTENSIONS OFF)
    qindaqt_enable_warnings(qindaqt_shell_launcher_qml)

    # AGENT-NOTE: The backing library, plugin, qmldir, and QML sources must
    # stay together in an installed prefix; the installed-package test stages
    # exactly this component plus the manifest and policy.
    install(
        TARGETS qindaqt_shell_launcher_qml qindaqt_shell_launcher_qmlplugin
        RUNTIME DESTINATION "${QT6_INSTALL_QML}/QindaQt/Shell/Launcher"
            COMPONENT LauncherAppletRuntime
        LIBRARY DESTINATION "${QT6_INSTALL_QML}/QindaQt/Shell/Launcher"
            COMPONENT LauncherAppletRuntime
        ARCHIVE DESTINATION "${QT6_INSTALL_QML}/QindaQt/Shell/Launcher"
            COMPONENT LauncherAppletRuntime
    )
    install(
        FILES
            "${QINDAQT_LAUNCHER_QML_OUTPUT_DIRECTORY}/qmldir"
            "${QINDAQT_LAUNCHER_QML_OUTPUT_DIRECTORY}/qindaqt_shell_launcher.qmltypes"
            "${module_dir}/qml/LauncherApplet.qml"
            "${module_dir}/qml/LauncherSection.qml"
        DESTINATION "${QT6_INSTALL_QML}/QindaQt/Shell/Launcher"
        COMPONENT LauncherAppletRuntime
    )
    install(
        FILES "${repo_root}/data/applets/launcher.json"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/qindaqt/applets"
        COMPONENT LauncherAppletRuntime
    )
    install(
        FILES "${repo_root}/data/applet-policy/default.json"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/qindaqt/applet-policy"
        COMPONENT LauncherAppletRuntime
    )
endfunction()

# Runtime registration: in the repository build this directory is processed
# before its dependencies, so registration defers to the end of the src scope
# (still before tests/). A focused standalone route that has already added
# the dependency targets sets QINDAQT_LAUNCHER_DEPENDENCIES_READY and gets
# the runtime immediately. Paths ride in global properties because deferred
# call arguments expand at execution time, in the deferred scope.
get_filename_component(QINDAQT_LAUNCHER_REPO_ROOT
                       "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
set_property(GLOBAL PROPERTY QINDAQT_LAUNCHER_MODULE_DIR
             "${CMAKE_CURRENT_SOURCE_DIR}")
set_property(GLOBAL PROPERTY QINDAQT_LAUNCHER_REPO_ROOT
             "${QINDAQT_LAUNCHER_REPO_ROOT}")
if(QINDAQT_LAUNCHER_DEPENDENCIES_READY)
    qindaqt_add_launcher_runtime()
else()
    cmake_language(
        DEFER DIRECTORY "${QINDAQT_LAUNCHER_REPO_ROOT}/src"
        CALL qindaqt_add_launcher_runtime
    )
endif()

# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_library(qindaqt_file_manager_support STATIC)
target_sources(
    qindaqt_file_manager_support
    PRIVATE
        app_shell/file_manager_action_catalog.cpp
        app_shell/file_manager_browsing_actions.cpp
        app_shell/file_manager_browsing_actions.h
        app_shell/file_manager_transfer_actions.cpp
        app_shell/file_manager_transfer_actions.h
        mutation/device_resolver.cpp
        mutation/home_trash.cpp
        mutation/local_mutation_backend.cpp
        mutation/mutation_controller.cpp
        mutation/mutation_controller_foreign.cpp
        mutation/mutation_types.cpp
        mutation/safe_path_operations.cpp
        mutation/safe_tree_operations.cpp
        preview/local_preview.cpp
        preview/preview_provider.cpp
        preview/theme_icon_provider.cpp
        model/bookmarks_store.cpp
        model/clipboard_controller.cpp
        model/entry_properties.cpp
        model/launch_intent.cpp
        model/listing_order.cpp
        model/local_directory_lister.cpp
        model/navigation_controller.cpp
        model/navigation_history.cpp
        model/places_controller.cpp
        model/search_controller.cpp
    PUBLIC
        app_shell/file_manager_action_catalog.h
        mutation/device_resolver.h
        mutation/home_trash.h
        mutation/local_mutation_backend.h
        mutation/mutation_backend.h
        mutation/mutation_controller.h
        mutation/mutation_types.h
        mutation/safe_path_operations.h
        mutation/safe_tree_operations.h
        preview/local_preview.h
        preview/preview_provider.h
        preview/theme_icon_provider.h
        model/bookmarks_store.h
        model/clipboard_controller.h
        model/directory_lister.h
        model/entry_properties.h
        model/file_manager_types.h
        model/launch_intent.h
        model/listing_order.h
        model/local_directory_lister.h
        model/navigation_controller.h
        model/navigation_history.h
        model/places_controller.h
        model/search_controller.h
)
target_include_directories(qindaqt_file_manager_support PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(
    qindaqt_file_manager_support
    PUBLIC QindaQt::AppShell QindaQt::AppShellMenuExport Qt6::Core Qt6::Gui Qt6::Quick
)
target_compile_features(qindaqt_file_manager_support PUBLIC cxx_std_20)
set_target_properties(qindaqt_file_manager_support PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_file_manager_support)

qt_add_executable(qindaqt-file-manager
    main.cpp
    runtime/mutation_ui_action_probe.cpp
    runtime/selection_ui_probe.cpp
    runtime/mutation_ui_action_probe.h
)
set_source_files_properties("${PROJECT_SOURCE_DIR}/data/artwork/empty-folder.png"
    PROPERTIES QT_RESOURCE_ALIAS empty-folder.png)
qt_add_resources(qindaqt-file-manager file_manager_artwork
    PREFIX /qindaqt/file-manager
    FILES "${PROJECT_SOURCE_DIR}/data/artwork/empty-folder.png")
qt_add_qml_module(
    qindaqt-file-manager
    URI QindaQt.FileManagerApp
    VERSION 1.0
    QML_FILES
        ui/IconButton.qml
        ui/PlaceButton.qml
        ui/StatusBanner.qml
        ui/Main.qml
        ui/Toolbar.qml
        ui/FilterBar.qml
        ui/FolderStatusBar.qml
        ui/Breadcrumb.qml
        ui/LocationBar.qml
        ui/PlacesSidebar.qml
        ui/EntrySelection.qml
        ui/EntryList.qml
        ui/EntryListDelegate.qml
        ui/EntryGrid.qml
        ui/ViewportScrollBar.qml
        ui/ViewportNavigation.qml
        ui/ZoomWheelHandler.qml
        ui/StatePane.qml
        ui/MutationDialogs.qml
        ui/PropertiesDialog.qml
        ui/EntryDrag.js
)

# ADR-0116: File Manager renders with stock Qt Quick Controls; it links only
# the non-visual seams (AppShell coordinator/catalog, menu export, F1 font
# bootstrap). No QindaQt.Tokens/Controls QML modules, no theme catalog, no
# per-app palette.
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

# AGENT-CONTRACT: The executable dynamically links libqindaqt_app_shell.so
# (the non-visual coordinator seam), whose backing library in turn needs the
# Controls/Tokens backing libraries. The staged FileManager component resolves
# that chain solely from its own payload: the executable's RPATH reaches the
# AppShell backing library, and the backing libraries' own INSTALL_RPATHs
# ($ORIGIN/../Tokens, $ORIGIN/../Controls) reach their siblings. Keep the
# install(TARGETS) rules below and this RPATH in sync.
target_link_libraries(
    qindaqt-file-manager
    PRIVATE
        qindaqt_file_manager_support
        QindaQt::AppShell
        QindaQt::FontDiscovery
        Qt6::Core
        Qt6::DBus
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
)
target_compile_features(qindaqt-file-manager PRIVATE cxx_std_20)
set_target_properties(
    qindaqt-file-manager
    PROPERTIES
        CXX_EXTENSIONS OFF
        INSTALL_RPATH
            "$ORIGIN/${qindaqt_file_manager_qml_relative_path}/QindaQt/AppShell"
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

# ADR-0116: the FileManager component ships no importable QindaQt QML modules
# (no qmldir, typeinfo, or QML sources) and no theme catalog. It still repeats
# the three backing shared libraries named by the executable's dynamic
# dependency chain — the same layout the pre-ADR-0116 component used — so a
# component-only staged install resolves libqindaqt_app_shell.so and its
# Controls/Tokens siblings without the rest of the shell install. The chain
# relies on each library's own INSTALL_RPATH; see the AGENT-CONTRACT above.
install(
    TARGETS qindaqt_app_shell
    LIBRARY DESTINATION "${QT6_INSTALL_QML}/QindaQt/AppShell"
    COMPONENT FileManager
)
install(
    TARGETS qindaqt_controls_qml
    LIBRARY DESTINATION "${QT6_INSTALL_QML}/QindaQt/Controls"
    COMPONENT FileManager
)
install(
    TARGETS qindaqt_tokens_qml
    LIBRARY DESTINATION "${QT6_INSTALL_QML}/QindaQt/Tokens"
    COMPONENT FileManager
)

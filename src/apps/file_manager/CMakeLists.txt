# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_library(qindaqt_file_manager_desktop_boundary STATIC)
target_sources(
    qindaqt_file_manager_desktop_boundary
    PRIVATE
        mutation/archive_codec.cpp
        mutation/device_resolver.cpp
        mutation/home_trash.cpp
        mutation/local_mutation_backend.cpp
        mutation/mutation_controller.cpp
        mutation/mutation_controller_foreign.cpp
        mutation/mutation_controller_items.cpp
        mutation/mutation_types.cpp
        mutation/safe_path_operations.cpp
        mutation/safe_tree_operations.cpp
        model/clipboard_controller.cpp
        model/launch_intent.cpp
        model/local_directory_lister.cpp
        public/desktop_file_boundary.cpp
    PUBLIC
        public/desktop_file_boundary.h
)
target_include_directories(
    qindaqt_file_manager_desktop_boundary PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}"
)
target_link_libraries(
    qindaqt_file_manager_desktop_boundary PUBLIC Qt6::Core Qt6::Gui
)
target_compile_features(qindaqt_file_manager_desktop_boundary PUBLIC cxx_std_20)
set_target_properties(
    qindaqt_file_manager_desktop_boundary PROPERTIES CXX_EXTENSIONS OFF
)
qindaqt_enable_warnings(qindaqt_file_manager_desktop_boundary)

# ADR-0260: File Manager's menu vocabulary (menu titles, action labels,
# descriptions, shortcuts) as pure values. File Manager's AppShell action
# catalog and the shell's desktop menu both project it, so the desktop and a
# File Manager window cannot drift apart. Only public/ is exported: a consumer
# of this target cannot reach File Manager's private models through it.
qt_add_library(qindaqt_file_manager_menu_catalog STATIC)
target_sources(
    qindaqt_file_manager_menu_catalog
    PRIVATE public/file_manager_menu_catalog.cpp
    PUBLIC public/file_manager_menu_catalog.h
)
target_include_directories(
    qindaqt_file_manager_menu_catalog PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/public"
)
target_link_libraries(qindaqt_file_manager_menu_catalog PUBLIC Qt6::Core Qt6::Gui)
target_compile_features(qindaqt_file_manager_menu_catalog PUBLIC cxx_std_20)
set_target_properties(
    qindaqt_file_manager_menu_catalog PROPERTIES CXX_EXTENSIONS OFF
)
qindaqt_enable_warnings(qindaqt_file_manager_menu_catalog)

qt_add_library(qindaqt_file_manager_support STATIC)
target_sources(
    qindaqt_file_manager_support
    PRIVATE
        app_shell/file_manager_action_catalog.cpp
        app_shell/file_manager_application_actions.cpp
        app_shell/file_manager_application_actions.h
        app_shell/file_manager_browsing_actions.cpp
        app_shell/file_manager_browsing_actions.h
        app_shell/file_manager_item_actions.cpp
        app_shell/file_manager_item_actions.h
        app_shell/file_manager_mutation_actions.cpp
        app_shell/file_manager_mutation_actions.h
        app_shell/file_manager_transfer_actions.cpp
        app_shell/file_manager_transfer_actions.h
        preview/local_preview.cpp
        preview/preview_provider.cpp
        preview/theme_icon_provider.cpp
        runtime/file_manager_application.cpp
        model/bookmarks_store.cpp
        model/clipboard_controller.cpp
        model/entry_properties.cpp
        model/file_templates.cpp
        model/folder_launch_controller.cpp
        model/listing_order.cpp
        model/open_with_controller.cpp
        model/open_with_launcher.cpp
        model/applications_controller.cpp
        model/applications_listing.cpp
        model/applications_place.cpp
        model/applications_place_order.cpp
        model/navigation_controller.cpp
        model/workspace_chooser_client.cpp
        model/navigation_history.cpp
        model/navigation_status.h
        model/places_controller.cpp
        model/preferences.cpp
        model/preferences_controller.cpp
        model/preferences_store.cpp
        model/search_controller.cpp
        model/state_file.cpp
        network/avahi_service_discovery.cpp
        network/connect_request.cpp
        network/discovery_controller.cpp
        network/kio_network_directory_backend.cpp
        network/kio_fuse_remote_file_opener.cpp
        network/kio_remote_file_opener.cpp
        network/kio_remote_folder_creator.cpp
        network/kio_remote_copier.cpp
        network/kio_remote_mover.cpp
        network/kio_remote_renamer.cpp
        network/kio_transfer_worker.cpp
        network/mount_unit.cpp
        network/network_location.cpp
        network/network_locations_controller.cpp
        network/network_locations_store.cpp
        network/network_mount_manager.cpp
        network/remote_copy_to_controller.cpp
        network/remote_create_folder_controller.cpp
        network/remote_move_to_controller.cpp
        network/remote_open_controller.cpp
        network/remote_rename_controller.cpp
        network/systemd_user_units.cpp
        network/transfer_queue_controller.cpp
        network/transfer_router.cpp
        network/transfer_types.cpp
        # ADR-0269: the only sources that name KArchive.
        mutation/karchive_codec.cpp
        mutation/karchive_codec_extract.cpp
        mutation/karchive_io.h
    PUBLIC
        app_shell/file_manager_action_catalog.h
        mutation/archive_codec.h
        mutation/device_resolver.h
        mutation/home_trash.h
        mutation/karchive_codec.h
        mutation/local_mutation_backend.h
        mutation/mutation_backend.h
        mutation/mutation_controller.h
        mutation/mutation_types.h
        mutation/safe_path_operations.h
        mutation/safe_tree_operations.h
        preview/local_preview.h
        preview/preview_provider.h
        preview/theme_icon_provider.h
        runtime/file_manager_application.h
        model/bookmarks_store.h
        model/applications_controller.h
        model/applications_listing.h
        model/applications_location.h
        model/applications_place.h
        model/applications_place_order.h
        model/clipboard_controller.h
        model/directory_lister.h
        model/entry_properties.h
        model/entry_presentation.h
        model/file_manager_types.h
        model/file_templates.h
        model/folder_launch_controller.h
        model/launch_intent.h
        model/listing_order.h
        model/local_directory_lister.h
        model/navigation_controller.h
        model/open_with_controller.h
        model/open_with_launcher.h
        model/workspace_chooser_client.h
        model/navigation_history.h
        model/places_controller.h
        model/preferences.h
        model/preferences_controller.h
        model/preferences_store.h
        model/search_controller.h
        model/state_file.h
        network/avahi_service_discovery.h
        network/connect_request.h
        network/discovery_controller.h
        network/kio_network_directory_backend.h
        network/kio_fuse_remote_file_opener.h
        network/kio_remote_file_opener.h
        network/kio_remote_folder_creator.h
        network/kio_remote_copier.h
        network/kio_remote_mover.h
        network/kio_remote_renamer.h
        network/kio_transfer_worker.h
        network/mount_unit.h
        network/network_directory_backend.h
        network/network_location.h
        network/network_locations_controller.h
        network/network_locations_store.h
        network/network_mount_manager.h
        network/remote_copy_to_controller.h
        network/remote_copier.h
        network/remote_create_folder_controller.h
        network/remote_file_opener.h
        network/remote_folder_creator.h
        network/remote_move_to_controller.h
        network/remote_mover.h
        network/remote_open_controller.h
        network/remote_rename_controller.h
        network/remote_renamer.h
        network/service_discovery.h
        network/systemd_user_units.h
        network/transfer_queue_controller.h
        network/transfer_router.h
        network/transfer_types.h
        network/transfer_worker.h
)
target_include_directories(qindaqt_file_manager_support PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
find_package(KF6KIO 6.0 REQUIRED CONFIG)
# ADR-0269: KArchive (a tier-1 framework beside the KIO this target already
# uses) backs Compress/Extract; packaging adds kde-frameworks/karchive:6.
find_package(KF6Archive 6.0 REQUIRED CONFIG)
target_link_libraries(
    qindaqt_file_manager_support
    PUBLIC
        qindaqt_file_manager_desktop_boundary
        qindaqt_file_manager_menu_catalog
        # ADR-0269: Open With reads and writes associations through Settings'
        # own Default Applications store; there is no second MIME authority.
        qindaqt_settings_default_apps
        QindaQt::AppShell
        QindaQt::AppShellMenuExport
        QindaQt::ApplicationCatalog
        Qt6::Core
        Qt6::Gui
        Qt6::Quick
    # ADR-0151: KIOWidgets registers KIO's standard job UI delegate at load
    # time; without it every KIO job has no credential/mount prompt path.
    # ADR-0152: KIOGui provides KIO::OpenUrlJob, the supported remote-open
    # facility. Qt6::Widgets is private because that delegate prompts with
    # QWidgets (Open With dialog, credential/message boxes); the process
    # application class lives in runtime/file_manager_application.h.
    # ADR-0157: Qt6::DBus is private for the session KIOFuse mount call; the
    # kio-fuse daemon lifecycle belongs to the platform, not File Manager.
    PRIVATE KF6::KIOCore KF6::KIOGui KF6::KIOWidgets KF6::Archive Qt6::Widgets Qt6::DBus
)
target_compile_features(qindaqt_file_manager_support PUBLIC cxx_std_20)
set_target_properties(qindaqt_file_manager_support PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_file_manager_support)

qt_add_executable(qindaqt-file-manager
    main.cpp
    runtime/file_actions_composition.cpp
    runtime/file_actions_composition.h
    runtime/mutation_ui_action_probe.cpp
    runtime/selection_ui_probe.cpp
    runtime/mutation_ui_action_probe.h
    runtime/ui_contract_probe.cpp
    runtime/ui_contract_probe.h
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
        ui/SelectionBand.qml
        ui/EntryList.qml
        ui/EntryListDelegate.qml
        ui/EntryGrid.qml
        ui/ViewportScrollBar.qml
        ui/ViewportNavigation.qml
        ui/ZoomWheelHandler.qml
        ui/StatePane.qml
        ui/ApplicationInfoDialog.qml
        ui/ApplicationsPlaceActions.qml
        ui/MutationDialogs.qml
        ui/StatusBanners.qml
        ui/ExportedMenuBar.qml
        ui/PresentationDefaults.qml
        ui/WindowServices.qml
        ui/NetworkHub.qml
        ui/NearbyServersSection.qml
        ui/NearbyServerRow.qml
        ui/PreferencesWindow.qml
        ui/PreferencesGeneralPage.qml
        ui/PreferencesViewsPage.qml
        ui/PreferencesNetworkPage.qml
        ui/PreferencesTrashPage.qml
        ui/NetworkLocationCard.qml
        ui/ConnectToServerDialog.qml
        ui/TransferQueueBanner.qml
        ui/PropertiesDialog.qml
        ui/FileContextMenu.qml
        ui/ContextActionItem.qml
        ui/ContextChoiceMenu.qml
        ui/ContextNewFileMenu.qml
        ui/ContextOpenWithMenu.qml
        ui/FileActions.qml
        ui/OpenWithDialog.qml
        ui/EntryDrag.js
    DEPENDENCIES
        QindaQt.Controls/1.0
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

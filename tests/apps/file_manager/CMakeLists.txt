# SPDX-License-Identifier: GPL-3.0-or-later

function(qindaqt_add_file_manager_test target source test_name)
    qt_add_executable(${target} ${source})
    target_link_libraries(${target} PRIVATE qindaqt_file_manager_support Qt6::Test)
    target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_compile_features(${target} PRIVATE cxx_std_20)
    set_target_properties(${target} PROPERTIES CXX_EXTENSIONS OFF)
    qindaqt_enable_warnings(${target})
    add_test(NAME ${test_name} COMMAND ${target})
    set_tests_properties(${test_name} PROPERTIES LABELS "file-manager")
endfunction()

qindaqt_add_file_manager_test(
    qindaqt_file_manager_history_tests
    tst_navigation_history.cpp
    qindaqt.file-manager-navigation-history
)
qindaqt_add_file_manager_test(
    qindaqt_file_manager_local_mutation_tests
    tst_local_mutation_backend.cpp
    qindaqt.file-manager-local-mutation
)
qindaqt_add_file_manager_test(
    qindaqt_file_manager_home_trash_tests
    tst_home_trash.cpp
    qindaqt.file-manager-home-trash
)
qindaqt_add_file_manager_test(
    qindaqt_file_manager_mutation_controller_tests
    tst_mutation_controller.cpp
    qindaqt.file-manager-mutation-controller
)
qindaqt_add_file_manager_test(
    qindaqt_file_manager_clipboard_controller_tests
    tst_clipboard_controller.cpp
    qindaqt.file-manager-clipboard-controller
)
# QClipboard requires a QGuiApplication; run it display-less.
set_tests_properties(qindaqt.file-manager-clipboard-controller PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QPA_PLATFORMTHEME=generic"
    ENVIRONMENT_MODIFICATION "DISPLAY=unset:;WAYLAND_DISPLAY=unset:")
qindaqt_add_file_manager_test(
    qindaqt_file_manager_recursive_search_tests
    tst_search_controller.cpp
    qindaqt.file-manager-recursive-search
)
qindaqt_add_file_manager_test(
    qindaqt_file_manager_entry_properties_tests
    tst_entry_properties.cpp
    qindaqt.file-manager-entry-properties
)

add_library(qindaqt_file_manager_race_writer SHARED race_writer_shim.cpp)
target_link_libraries(qindaqt_file_manager_race_writer PRIVATE ${CMAKE_DL_LIBS})
target_compile_features(qindaqt_file_manager_race_writer PRIVATE cxx_std_20)
set_target_properties(
    qindaqt_file_manager_race_writer
    PROPERTIES CXX_EXTENSIONS OFF PREFIX ""
)
qindaqt_enable_warnings(qindaqt_file_manager_race_writer)

qindaqt_add_file_manager_test(
    qindaqt_file_manager_no_replace_race_tests
    tst_no_replace_race.cpp
    qindaqt.file-manager-no-replace-race
)
set_tests_properties(
    qindaqt.file-manager-no-replace-race
    PROPERTIES
        ENVIRONMENT
            "QINDAQT_RACE_DESTINATION=dest;LD_PRELOAD=$<TARGET_FILE:qindaqt_file_manager_race_writer>"
)
qindaqt_add_file_manager_test(
    qindaqt_file_manager_action_catalog_tests
    tst_file_manager_action_catalog.cpp
    qindaqt.file-manager-action-catalog
)
qindaqt_add_file_manager_test(
    qindaqt_file_manager_local_lister_tests
    tst_local_directory_lister.cpp
    qindaqt.file-manager-local-lister
)
qindaqt_add_file_manager_test(
    qindaqt_file_manager_launch_intent_tests
    tst_launch_intent.cpp
    qindaqt.file-manager-launch-intent
)
qindaqt_add_file_manager_test(
    qindaqt_file_manager_controller_tests
    tst_navigation_controller.cpp
    qindaqt.file-manager-navigation-controller
)
qindaqt_add_file_manager_test(
    qindaqt_file_manager_network_location_tests
    tst_network_location.cpp
    qindaqt.file-manager-network-location
)
# ADR-0194: the saved-locations schema, its refusals, and the pure policy
# that turns Connect-to-server dialog text into one canonical location.
qindaqt_add_file_manager_test(
    qindaqt_file_manager_network_locations_store_tests
    tst_network_locations_store.cpp
    qindaqt.file-manager-network-locations-store
)
qindaqt_add_file_manager_test(
    qindaqt_file_manager_connect_request_tests
    tst_connect_request.cpp
    qindaqt.file-manager-connect-request
)
qindaqt_add_file_manager_test(
    qindaqt_file_manager_network_locations_controller_tests
    tst_network_locations_controller.cpp
    qindaqt.file-manager-network-locations-controller
)
# ADR-0200: Avahi browsing and the Nearby model. The production browser's
# D-Bus edges are answered with canned replies and its browser signals are
# delivered directly, so no bus, daemon, or network is involved.
qindaqt_add_file_manager_test(
    qindaqt_file_manager_avahi_discovery_tests
    tst_avahi_service_discovery.cpp
    qindaqt.file-manager-avahi-discovery
)
target_link_libraries(
    qindaqt_file_manager_avahi_discovery_tests PRIVATE Qt6::DBus
)
qindaqt_add_file_manager_test(
    qindaqt_file_manager_discovery_controller_tests
    tst_discovery_controller.cpp
    qindaqt.file-manager-discovery-controller
)
# ADR-0198: the durable preference schema and the controller that owns it.
qindaqt_add_file_manager_test(
    qindaqt_file_manager_preferences_store_tests
    tst_preferences_store.cpp
    qindaqt.file-manager-preferences-store
)
qindaqt_add_file_manager_test(
    qindaqt_file_manager_preferences_controller_tests
    tst_preferences_controller.cpp
    qindaqt.file-manager-preferences-controller
)
# ADR-0199: the mount unit text and name, and the manager that keeps the unit
# directory in step with the saved locations. No systemctl is ever run.
qindaqt_add_file_manager_test(
    qindaqt_file_manager_mount_unit_tests
    tst_mount_unit.cpp
    qindaqt.file-manager-mount-unit
)
qindaqt_add_file_manager_test(
    qindaqt_file_manager_mount_manager_tests
    tst_network_mount_manager.cpp
    qindaqt.file-manager-mount-manager
)
# ADR-0195: the routing policy and the queue's sequencing, fencing, pause,
# and cancellation against a recording worker double -- no KIO, no network.
qindaqt_add_file_manager_test(
    qindaqt_file_manager_transfer_router_tests
    tst_transfer_router.cpp
    qindaqt.file-manager-transfer-router
)
qindaqt_add_file_manager_test(
    qindaqt_file_manager_transfer_queue_tests
    tst_transfer_queue.cpp
    qindaqt.file-manager-transfer-queue
)
# The production worker's realm boundary and retained KIO UI delegate via a
# job-creation test seam.
qindaqt_add_file_manager_test(
    qindaqt_file_manager_kio_transfer_worker_tests
    tst_kio_transfer_worker.cpp
    qindaqt.file-manager-kio-transfer-worker
)
target_link_libraries(
    qindaqt_file_manager_kio_transfer_worker_tests
    PRIVATE KF6::KIOCore
)
qindaqt_add_file_manager_test(
    qindaqt_file_manager_controller_network_tests
    tst_navigation_controller_network.cpp
    qindaqt.file-manager-navigation-controller-network
)
qindaqt_add_file_manager_test(
    qindaqt_file_manager_controller_network_mutation_tests
    tst_navigation_controller_network_mutation.cpp
    qindaqt.file-manager-navigation-controller-network-mutation
)
qindaqt_add_file_manager_test(
    qindaqt_file_manager_kio_network_backend_tests
    tst_kio_network_directory_backend.cpp
    qindaqt.file-manager-kio-network-backend
)
# The delegate-seam probes include KIO job headers directly; the support
# library keeps KIO private, so the includes come from the KIO target here.
find_package(KF6KIO 6.0 REQUIRED CONFIG)
target_link_libraries(
    qindaqt_file_manager_kio_network_backend_tests
    PRIVATE KF6::KIOCore
)
qindaqt_add_file_manager_test(
    qindaqt_file_manager_kio_remote_opener_tests
    tst_kio_remote_file_opener.cpp
    qindaqt.file-manager-kio-remote-opener
)
target_link_libraries(
    qindaqt_file_manager_kio_remote_opener_tests
    PRIVATE KF6::KIOGui Qt6::Widgets
)
# The P1 regression row drives KIO's standard Open With prompt, which needs a
# QWidget-capable application; run display-less like the other GUI rows.
set_tests_properties(qindaqt.file-manager-kio-remote-opener PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QPA_PLATFORMTHEME=generic"
    ENVIRONMENT_MODIFICATION "DISPLAY=unset:;WAYLAND_DISPLAY=unset:")
qindaqt_add_file_manager_test(
    qindaqt_file_manager_kio_fuse_remote_opener_tests
    tst_kio_fuse_remote_file_opener.cpp
    qindaqt.file-manager-kio-fuse-remote-opener
)
# The seams include KIO job and QtDBus headers directly; the support library
# keeps both private, so the includes come from the targets here.
target_link_libraries(
    qindaqt_file_manager_kio_fuse_remote_opener_tests
    PRIVATE KF6::KIOGui Qt6::Widgets Qt6::DBus
)
# Rows that build a real resolved-open job need the QWidget-capable
# application but no display, session bus, or network; the faked mount seam
# answers every resolution in-process.
set_tests_properties(qindaqt.file-manager-kio-fuse-remote-opener PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QPA_PLATFORMTHEME=generic"
    ENVIRONMENT_MODIFICATION "DISPLAY=unset:;WAYLAND_DISPLAY=unset:;DBUS_SESSION_BUS_ADDRESS=unset:")
qindaqt_add_file_manager_test(
    qindaqt_file_manager_kio_remote_renamer_tests
    tst_kio_remote_renamer.cpp
    qindaqt.file-manager-kio-remote-renamer
)
target_link_libraries(
    qindaqt_file_manager_kio_remote_renamer_tests
    PRIVATE KF6::KIOCore
)
qindaqt_add_file_manager_test(
    qindaqt_file_manager_kio_remote_folder_creator_tests
    tst_kio_remote_folder_creator.cpp
    qindaqt.file-manager-kio-remote-folder-creator
)
target_link_libraries(
    qindaqt_file_manager_kio_remote_folder_creator_tests
    PRIVATE KF6::KIOCore
)
qindaqt_add_file_manager_test(
    qindaqt_file_manager_kio_remote_copier_tests
    tst_kio_remote_copier.cpp
    qindaqt.file-manager-kio-remote-copier
)
target_link_libraries(
    qindaqt_file_manager_kio_remote_copier_tests
    PRIVATE KF6::KIOCore
)
# ADR-0156: the production KioRemoteMover's scheme/authority boundary and
# retained KIO UI delegate via a job-creation test seam.
qindaqt_add_file_manager_test(
    qindaqt_file_manager_kio_remote_mover_tests
    tst_kio_remote_mover.cpp
    qindaqt.file-manager-kio-remote-mover
)
target_link_libraries(
    qindaqt_file_manager_kio_remote_mover_tests
    PRIVATE KF6::KIOCore
)
# ADR-0156: fake-injected remote Move To dispatch/validation/refresh/fence
# rows, split from the copy rows because a confirmed move always refreshes
# the visible folder.
qindaqt_add_file_manager_test(
    qindaqt_file_manager_remote_move_dispatch_tests
    tst_remote_move_dispatch.cpp
    qindaqt.file-manager-remote-move-dispatch
)
qindaqt_add_file_manager_test(
    qindaqt_file_manager_mutation_action_binding_tests
    tst_mutation_action_binding.cpp
    qindaqt.file-manager-mutation-action-binding
)
qindaqt_add_file_manager_test(
    qindaqt_file_manager_listing_order_tests
    tst_listing_order.cpp
    qindaqt.file-manager-listing-order
)
qindaqt_add_file_manager_test(
    qindaqt_file_manager_bookmarks_store_tests
    tst_bookmarks_store.cpp
    qindaqt.file-manager-bookmarks-store
)
qindaqt_add_file_manager_test(
    qindaqt_file_manager_places_controller_tests
    tst_places_controller.cpp
    qindaqt.file-manager-places-controller
)
qt_add_executable(
    qindaqt_file_manager_desktop_file_boundary_tests
    tst_desktop_file_boundary.cpp
)
target_link_libraries(
    qindaqt_file_manager_desktop_file_boundary_tests
    PRIVATE qindaqt_file_manager_desktop_boundary Qt6::Test
)
target_compile_features(
    qindaqt_file_manager_desktop_file_boundary_tests PRIVATE cxx_std_20
)
set_target_properties(
    qindaqt_file_manager_desktop_file_boundary_tests PROPERTIES CXX_EXTENSIONS OFF
)
qindaqt_enable_warnings(qindaqt_file_manager_desktop_file_boundary_tests)
add_test(
    NAME qindaqt.file-manager-desktop-file-boundary
    COMMAND qindaqt_file_manager_desktop_file_boundary_tests
)
set_tests_properties(
    qindaqt.file-manager-desktop-file-boundary PROPERTIES LABELS "file-manager"
)

# ADR-0273: "Show in folder" -- the URI and path policy every
# org.freedesktop.FileManager1 call passes before a window is shown.
qindaqt_add_file_manager_test(
    qindaqt_file_manager_reveal_request_tests
    tst_reveal_request.cpp
    qindaqt.file-manager-reveal-request
)

# ADR-0273: org.freedesktop.FileManager1 on a private session bus with a
# recording window factory, and the production factory's window reuse rule.
# The bus configuration has no activation directories, so the row can never
# start the host's own file manager.
find_program(QINDAQT_DBUS_RUN_SESSION dbus-run-session REQUIRED)
qt_add_executable(qindaqt_file_manager_file_manager1_tests tst_file_manager1_service.cpp)
target_link_libraries(
    qindaqt_file_manager_file_manager1_tests
    PRIVATE qindaqt_file_manager_support Qt6::DBus Qt6::Gui Qt6::Test
)
target_include_directories(
    qindaqt_file_manager_file_manager1_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}"
)
target_compile_features(qindaqt_file_manager_file_manager1_tests PRIVATE cxx_std_20)
set_target_properties(
    qindaqt_file_manager_file_manager1_tests PROPERTIES CXX_EXTENSIONS OFF
)
qindaqt_enable_warnings(qindaqt_file_manager_file_manager1_tests)
add_test(
    NAME qindaqt.file-manager-file-manager1
    COMMAND "${QINDAQT_DBUS_RUN_SESSION}"
            "--config-file=${CMAKE_CURRENT_SOURCE_DIR}/file_manager1_bus.conf"
            -- $<TARGET_FILE:qindaqt_file_manager_file_manager1_tests>
)
set_tests_properties(
    qindaqt.file-manager-file-manager1
    PROPERTIES
        LABELS "file-manager"
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        ENVIRONMENT_MODIFICATION "DISPLAY=unset:;WAYLAND_DISPLAY=unset:"
)

# ADR-0273: Keep in Dock over the dock's public pin helper (ADR-0265) and the
# application.keep-in-dock action, against a scripted Settings1 transport.
qindaqt_add_file_manager_test(
    qindaqt_file_manager_application_dock_pins_tests
    tst_application_dock_pins.cpp
    qindaqt.file-manager-application-dock-pins
)
target_link_libraries(
    qindaqt_file_manager_application_dock_pins_tests PRIVATE QindaQt::DockPins
)
set_tests_properties(qindaqt.file-manager-application-dock-pins PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_FATAL_WARNINGS=1"
    ENVIRONMENT_MODIFICATION "DISPLAY=unset:;WAYLAND_DISPLAY=unset:;DBUS_SESSION_BUS_ADDRESS=unset:")

add_test(
    NAME qindaqt.file-manager-desktop-metadata
    COMMAND "${CMAKE_COMMAND}"
            "-DDESKTOP_FILE=${PROJECT_SOURCE_DIR}/src/apps/file_manager/org.qindaqt.FileManager.desktop"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/check_desktop_metadata.cmake"
)
set_tests_properties(qindaqt.file-manager-desktop-metadata PROPERTIES LABELS "file-manager")

add_test(
    NAME qindaqt.file-manager-cli-rejects-multiple-paths
    COMMAND "${CMAKE_COMMAND}"
            "-DFILE_MANAGER_EXECUTABLE=$<TARGET_FILE:qindaqt-file-manager>"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/check_cli_rejection.cmake"
)
set_tests_properties(
    qindaqt.file-manager-cli-rejects-multiple-paths
    PROPERTIES LABELS "file-manager" ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

add_test(
    NAME qindaqt.file-manager-mutation-ui-offscreen
    COMMAND $<TARGET_FILE:qindaqt-file-manager>
            --check-ui-contract "${CMAKE_BINARY_DIR}"
)
set_tests_properties(
    qindaqt.file-manager-mutation-ui-offscreen
    PROPERTIES
        LABELS "file-manager"
        ENVIRONMENT
            "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_FATAL_WARNINGS=1;QT_QPA_PLATFORMTHEME=generic;QT_QUICK_CONTROLS_STYLE=Fusion;XDG_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/ui-data"
        ENVIRONMENT_MODIFICATION
            "DISPLAY=unset:;WAYLAND_DISPLAY=unset:;DBUS_SESSION_BUS_ADDRESS=unset:"
)

# AGENT-NOTE: Review P1-1 was invisible to the construction-only row above.
# Keep this separate row driving real AppShell actions through production QML
# and back into the local backend against a build-root fixture.
add_test(
    NAME qindaqt.file-manager-mutation-ui-actions-offscreen
    COMMAND $<TARGET_FILE:qindaqt-file-manager>
            --check-ui-actions "${CMAKE_BINARY_DIR}"
)
set_tests_properties(
    qindaqt.file-manager-mutation-ui-actions-offscreen
    PROPERTIES
        LABELS "file-manager"
        ENVIRONMENT
            "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_FATAL_WARNINGS=1;QT_QPA_PLATFORMTHEME=generic;QT_QUICK_CONTROLS_STYLE=Fusion;XDG_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/ui-action-data"
        ENVIRONMENT_MODIFICATION
            "DISPLAY=unset:;WAYLAND_DISPLAY=unset:;DBUS_SESSION_BUS_ADDRESS=unset:"
)

add_test(
    NAME qindaqt.file-manager-mutation-boundary
    COMMAND "${CMAKE_COMMAND}"
            "-DSOURCE_ROOT=${PROJECT_SOURCE_DIR}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/check_mutation_boundary.cmake"
)
set_tests_properties(
    qindaqt.file-manager-mutation-boundary
    PROPERTIES LABELS "file-manager;policy"
)

add_test(
    NAME qindaqt.file-manager-cli-rejects-non-folder
    COMMAND "${CMAKE_COMMAND}"
            "-DFILE_MANAGER_EXECUTABLE=$<TARGET_FILE:qindaqt-file-manager>"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/check_invalid_folder.cmake"
)
set_tests_properties(
    qindaqt.file-manager-cli-rejects-non-folder
    PROPERTIES LABELS "file-manager" ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

# AGENT-CONTRACT: This row stages only the FileManager component and clears
# ambient build/import/library lookup. Its probes must therefore load the
# executable and the real QML root solely from the disposable relocatable
# prefix. ADR-0116: the component no longer carries importable Tokens/Controls
# QML modules or the per-app theme catalog; the row asserts their absence and
# requires the AppShell/Controls/Tokens backing shared libraries the
# executable's dynamic dependency chain still needs.
add_test(
    NAME qindaqt.file-manager-installed-runtime
    COMMAND
        "${CMAKE_COMMAND}"
        "-DQINDAQT_CMAKE=${CMAKE_COMMAND}"
        "-DQINDAQT_BUILD_DIRECTORY=${CMAKE_BINARY_DIR}"
        "-DQINDAQT_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/installed-file-manager-stage"
        "-DQINDAQT_INSTALL_BINDIR=${CMAKE_INSTALL_BINDIR}"
        "-DQINDAQT_INSTALL_DATADIR=${CMAKE_INSTALL_DATADIR}"
        "-DQINDAQT_QML_INSTALL_DIR=${QT6_INSTALL_QML}"
        "-DQINDAQT_CONFIGURATION=$<CONFIG>"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_installed_file_manager.cmake"
)
set_tests_properties(
    qindaqt.file-manager-installed-runtime
    PROPERTIES LABELS "file-manager;package" RUN_SERIAL TRUE TIMEOUT 180
)

add_test(NAME qindaqt.file-manager-selection-qml
    COMMAND $<TARGET_FILE:Qt6::qmltestrunner>
        -input "${CMAKE_CURRENT_SOURCE_DIR}/qml/tst_selection.qml")
set_tests_properties(qindaqt.file-manager-selection-qml PROPERTIES
    LABELS "file-manager"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_QUICK_CONTROLS_STYLE=Fusion"
    ENVIRONMENT_MODIFICATION "DISPLAY=unset:;WAYLAND_DISPLAY=unset:;DBUS_SESSION_BUS_ADDRESS=unset:")

qindaqt_add_file_manager_test(
    qindaqt_file_manager_local_preview_tests
    tst_local_preview.cpp
    qindaqt.file-manager-local-preview
)
qindaqt_add_file_manager_test(
    qindaqt_file_manager_preview_provider_tests
    tst_preview_provider.cpp
    qindaqt.file-manager-preview-provider
)

set_tests_properties(qindaqt.file-manager-preview-provider PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software"
    ENVIRONMENT_MODIFICATION "DISPLAY=unset:;WAYLAND_DISPLAY=unset:;DBUS_SESSION_BUS_ADDRESS=unset:")

# Symbolic glyphs must follow the palette foreground so they stay readable on
# dark themes; full-color artwork must never be recolored.
qindaqt_add_file_manager_test(
    qindaqt_file_manager_theme_icon_provider_tests
    tst_theme_icon_provider.cpp
    qindaqt.file-manager-theme-icon-provider
)
target_compile_definitions(qindaqt_file_manager_theme_icon_provider_tests PRIVATE
    QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
set_tests_properties(qindaqt.file-manager-theme-icon-provider PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_QPA_PLATFORMTHEME=generic"
    ENVIRONMENT_MODIFICATION "DISPLAY=unset:;WAYLAND_DISPLAY=unset:;DBUS_SESSION_BUS_ADDRESS=unset:")

qt_add_executable(qindaqt_file_manager_visual_probe visual_probe.cpp)
target_link_libraries(qindaqt_file_manager_visual_probe PRIVATE
    qindaqt_file_manager_support Qt6::Quick)
target_compile_features(qindaqt_file_manager_visual_probe PRIVATE cxx_std_20)
qindaqt_enable_warnings(qindaqt_file_manager_visual_probe)
set_source_files_properties("${PROJECT_SOURCE_DIR}/data/artwork/empty-folder.png"
    PROPERTIES QT_RESOURCE_ALIAS empty-folder.png)
qt_add_resources(qindaqt_file_manager_visual_probe file_manager_probe_artwork
    PREFIX /qindaqt/file-manager
    FILES "${PROJECT_SOURCE_DIR}/data/artwork/empty-folder.png")

# QTEST_MAIN instantiates QGuiApplication through the public AppShell/Controls
# dependency; even a pure action-catalog test needs a display-less backend.
set_tests_properties(qindaqt.file-manager-action-catalog PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_FATAL_WARNINGS=1")

# Exercise actual window shortcuts and wheel delivery against local fixtures.
qindaqt_add_file_manager_test(qindaqt_file_manager_browsing_ui_tests
    tst_browsing_ui.cpp qindaqt.file-manager-browsing-ui)
target_link_libraries(qindaqt_file_manager_browsing_ui_tests PRIVATE Qt6::Quick)
target_compile_definitions(qindaqt_file_manager_browsing_ui_tests PRIVATE
    QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
set_tests_properties(qindaqt.file-manager-browsing-ui PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_FATAL_WARNINGS=1;QT_QPA_PLATFORMTHEME=generic;QT_QUICK_CONTROLS_STYLE=Fusion"
    ENVIRONMENT_MODIFICATION "DISPLAY=unset:;WAYLAND_DISPLAY=unset:;DBUS_SESSION_BUS_ADDRESS=unset:")

# Review P1 repair coverage: the production coordinator -> Main.qml ->
# MutationDialogs route for the remote one-child Copy contract and the
# shared Cancel action's remote-owner routing, all over injected fakes.
qindaqt_add_file_manager_test(qindaqt_file_manager_remote_copy_guard_tests
    tst_remote_copy_guard.cpp qindaqt.file-manager-remote-copy-guard)
target_link_libraries(qindaqt_file_manager_remote_copy_guard_tests PRIVATE Qt6::Quick)
target_compile_definitions(qindaqt_file_manager_remote_copy_guard_tests PRIVATE
    QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
set_tests_properties(qindaqt.file-manager-remote-copy-guard PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_FATAL_WARNINGS=1;QT_QPA_PLATFORMTHEME=generic;QT_QUICK_CONTROLS_STYLE=Fusion"
    ENVIRONMENT_MODIFICATION "DISPLAY=unset:;WAYLAND_DISPLAY=unset:;DBUS_SESSION_BUS_ADDRESS=unset:")
qt_add_resources(qindaqt_file_manager_remote_copy_guard_tests file_manager_remote_copy_guard_artwork
    PREFIX /qindaqt/file-manager
    FILES "${PROJECT_SOURCE_DIR}/data/artwork/empty-folder.png")

# ADR-0194: the production coordinator -> Main.qml route for the Network hub
# and the Connect-to-server dialog, over injected fakes and a temporary state
# directory. Asserts real rendering (parent chain to the window plus positive
# size), not merely existence.
qindaqt_add_file_manager_test(qindaqt_file_manager_network_hub_ui_tests
    tst_network_hub_ui.cpp qindaqt.file-manager-network-hub-ui)
target_link_libraries(qindaqt_file_manager_network_hub_ui_tests PRIVATE Qt6::Quick)
target_compile_definitions(qindaqt_file_manager_network_hub_ui_tests PRIVATE
    QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
set_tests_properties(qindaqt.file-manager-network-hub-ui PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_FATAL_WARNINGS=1;QT_QPA_PLATFORMTHEME=generic;QT_QUICK_CONTROLS_STYLE=Fusion"
    ENVIRONMENT_MODIFICATION "DISPLAY=unset:;WAYLAND_DISPLAY=unset:;DBUS_SESSION_BUS_ADDRESS=unset:")
qt_add_resources(qindaqt_file_manager_network_hub_ui_tests file_manager_network_hub_artwork
    PREFIX /qindaqt/file-manager
    FILES "${PROJECT_SOURCE_DIR}/data/artwork/empty-folder.png")

qindaqt_add_file_manager_test(qindaqt_file_manager_name_filter_tests
    tst_name_filter.cpp qindaqt.file-manager-name-filter)
qindaqt_add_file_manager_test(qindaqt_file_manager_icon_zoom_tests
    tst_icon_zoom.cpp qindaqt.file-manager-icon-zoom)

find_package(Qt6 6.11 REQUIRED COMPONENTS QuickTest)
qt_add_executable(qindaqt_file_manager_viewport_tests viewport_quick_test.cpp)
target_link_libraries(qindaqt_file_manager_viewport_tests PRIVATE
    Qt6::QuickTest Qt6::Quick qindaqt_file_manager_support)
target_compile_definitions(qindaqt_file_manager_viewport_tests PRIVATE
    QINDAQT_SOURCE_ROOT="${PROJECT_SOURCE_DIR}")
add_test(NAME qindaqt.file-manager-viewport
    COMMAND qindaqt_file_manager_viewport_tests
        -input "${CMAKE_CURRENT_SOURCE_DIR}/qml/tst_viewport.qml")
set_tests_properties(qindaqt.file-manager-viewport PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_FATAL_WARNINGS=1;QT_QUICK_CONTROLS_STYLE=Fusion"
    ENVIRONMENT_MODIFICATION "DISPLAY=unset:;WAYLAND_DISPLAY=unset:;DBUS_SESSION_BUS_ADDRESS=unset:"
    LABELS "file-manager;offscreen;accessibility")

qt_add_resources(qindaqt_file_manager_browsing_ui_tests file_manager_browsing_artwork
    PREFIX /qindaqt/file-manager
    FILES "${PROJECT_SOURCE_DIR}/data/artwork/empty-folder.png")
qindaqt_add_file_manager_test(
    qindaqt_file_manager_applications_controller_tests
    tst_applications_controller.cpp
    qindaqt.file-manager-applications-controller
)
# ADR-0262: the Applications place browsed by the ordinary NavigationController
# through its lister/launcher decorators, over a fixture catalog and a
# recording launch seam (no bus, nothing spawned).
qindaqt_add_file_manager_test(
    qindaqt_file_manager_applications_place_tests
    tst_applications_place.cpp
    qindaqt.file-manager-applications-place
)
# ADR-0262: production Main.qml in the Applications place -- selection,
# double-click/Enter opening, type-to-select, filter, details view, grouping,
# context actions, chooser mode and the docked replacement route.
qindaqt_add_file_manager_test(qindaqt_file_manager_applications_ui_tests
    tst_applications_ui.cpp qindaqt.file-manager-applications-ui)
target_link_libraries(qindaqt_file_manager_applications_ui_tests PRIVATE Qt6::Quick)
target_compile_definitions(qindaqt_file_manager_applications_ui_tests PRIVATE
    QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
set_tests_properties(qindaqt.file-manager-applications-ui PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_FATAL_WARNINGS=1;QT_QPA_PLATFORMTHEME=generic;QT_QUICK_CONTROLS_STYLE=Fusion"
    ENVIRONMENT_MODIFICATION "DISPLAY=unset:;WAYLAND_DISPLAY=unset:;DBUS_SESSION_BUS_ADDRESS=unset:")
qt_add_resources(qindaqt_file_manager_applications_ui_tests file_manager_applications_ui_artwork
    PREFIX /qindaqt/file-manager
    FILES "${PROJECT_SOURCE_DIR}/data/artwork/empty-folder.png")
# ADR-0269: the right-click set's rows live in their own file (after the
# artwork's resource alias above is set).
include("${CMAKE_CURRENT_SOURCE_DIR}/FileActionsTests.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/ViewsTests.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/EverydayTests.cmake")

# AGENT-GUARD: these rows construct a QGuiApplication, so they must name a
# platform. Without this they default to xcb and abort before their first
# assertion on any machine with no display -- a headless runner, or a
# developer on a VT -- which reads as a real failure and is not one. Every
# one of them passes under offscreen. Keep this list in step with the rows
# above; a new GUI row added without it fails the same way.
set_tests_properties(
    qindaqt.file-manager-connect-request
    qindaqt.file-manager-discovery-controller
    qindaqt.file-manager-mount-manager
    qindaqt.file-manager-mutation-action-binding
    qindaqt.file-manager-network-locations-controller
    qindaqt.file-manager-network-locations-store
    qindaqt.file-manager-preferences-controller
    qindaqt.file-manager-transfer-queue
    qindaqt.file-manager-transfer-router
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

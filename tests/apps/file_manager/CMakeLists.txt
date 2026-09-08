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
            --theme-directory "${PROJECT_SOURCE_DIR}/data/themes"
            --check-ui-contract "${CMAKE_BINARY_DIR}"
)
set_tests_properties(
    qindaqt.file-manager-mutation-ui-offscreen
    PROPERTIES
        LABELS "file-manager"
        ENVIRONMENT
            "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_FATAL_WARNINGS=1;QT_QPA_PLATFORMTHEME=generic;XDG_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/ui-data"
        ENVIRONMENT_MODIFICATION
            "DISPLAY=unset:;WAYLAND_DISPLAY=unset:;DBUS_SESSION_BUS_ADDRESS=unset:"
)

# AGENT-NOTE: Review P1-1 was invisible to the construction-only row above.
# Keep this separate row driving real AppShell actions through production QML
# and back into the local backend against a build-root fixture.
add_test(
    NAME qindaqt.file-manager-mutation-ui-actions-offscreen
    COMMAND $<TARGET_FILE:qindaqt-file-manager>
            --theme-directory "${PROJECT_SOURCE_DIR}/data/themes"
            --check-ui-actions "${CMAKE_BINARY_DIR}"
)
set_tests_properties(
    qindaqt.file-manager-mutation-ui-actions-offscreen
    PROPERTIES
        LABELS "file-manager"
        ENVIRONMENT
            "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_FATAL_WARNINGS=1;QT_QPA_PLATFORMTHEME=generic;XDG_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/ui-action-data"
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

get_target_property(
    qindaqt_file_manager_expected_controls_paths
    qindaqt_controls_qml
    QINDAQT_QML_DEPLOY_PATHS
)
string(REPLACE ";" "|" qindaqt_file_manager_expected_controls_paths
       "${qindaqt_file_manager_expected_controls_paths}")

# AGENT-CONTRACT: This row stages only the FileManager component and clears
# ambient build/import/library lookup. Its two probes must therefore load the
# executable, themes, Tokens, Controls, and the real QML root solely from the
# disposable relocatable prefix.
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
        "-DQINDAQT_TOKENS_LIBRARY_NAME=$<TARGET_FILE_NAME:qindaqt_tokens_qml>"
        "-DQINDAQT_TOKENS_PLUGIN_NAME=$<TARGET_FILE_NAME:qindaqt_tokens_qmlplugin>"
        "-DQINDAQT_CONTROLS_LIBRARY_NAME=$<TARGET_FILE_NAME:qindaqt_controls_qml>"
        "-DQINDAQT_CONTROLS_PLUGIN_NAME=$<TARGET_FILE_NAME:qindaqt_controls_qmlplugin>"
        "-DQINDAQT_EXPECTED_CONTROLS_PATHS=${qindaqt_file_manager_expected_controls_paths}"
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
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software"
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

qt_add_executable(qindaqt_file_manager_visual_probe visual_probe.cpp)
target_link_libraries(qindaqt_file_manager_visual_probe PRIVATE
    qindaqt_file_manager_support QindaQt::TokensQml qindaqt_themes Qt6::Quick)
target_compile_features(qindaqt_file_manager_visual_probe PRIVATE cxx_std_20)
qindaqt_enable_warnings(qindaqt_file_manager_visual_probe)
add_dependencies(qindaqt_file_manager_visual_probe qindaqt_controls_qmlplugin qindaqt_tokens_qmlplugin qindaqt_app_shellplugin)
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
target_link_libraries(qindaqt_file_manager_browsing_ui_tests PRIVATE
    QindaQt::TokensQml qindaqt_themes Qt6::Quick)
target_compile_definitions(qindaqt_file_manager_browsing_ui_tests PRIVATE
    QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
    QINDAQT_QML_IMPORT_PATH="${CMAKE_BINARY_DIR}/qml")
add_dependencies(qindaqt_file_manager_browsing_ui_tests
    qindaqt_controls_qmlplugin qindaqt_tokens_qmlplugin qindaqt_app_shellplugin)
set_tests_properties(qindaqt.file-manager-browsing-ui PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_FATAL_WARNINGS=1;QT_QPA_PLATFORMTHEME=generic"
    ENVIRONMENT_MODIFICATION "DISPLAY=unset:;WAYLAND_DISPLAY=unset:;DBUS_SESSION_BUS_ADDRESS=unset:")

qindaqt_add_file_manager_test(qindaqt_file_manager_name_filter_tests
    tst_name_filter.cpp qindaqt.file-manager-name-filter)
qindaqt_add_file_manager_test(qindaqt_file_manager_icon_zoom_tests
    tst_icon_zoom.cpp qindaqt.file-manager-icon-zoom)

find_package(Qt6 6.11 REQUIRED COMPONENTS QuickTest)
qt_add_executable(qindaqt_file_manager_viewport_tests viewport_quick_test.cpp)
target_link_libraries(qindaqt_file_manager_viewport_tests PRIVATE
    Qt6::QuickTest Qt6::Quick QindaQt::TokensQml qindaqt_themes)
target_compile_definitions(qindaqt_file_manager_viewport_tests PRIVATE
    QINDAQT_SOURCE_ROOT="${PROJECT_SOURCE_DIR}"
    QINDAQT_QML_IMPORT_PATH="${CMAKE_BINARY_DIR}/qml")
add_dependencies(qindaqt_file_manager_viewport_tests qindaqt_controls_qmlplugin qindaqt_tokens_qmlplugin)
add_test(NAME qindaqt.file-manager-viewport
    COMMAND qindaqt_file_manager_viewport_tests
        -input "${CMAKE_CURRENT_SOURCE_DIR}/qml/tst_viewport.qml")
set_tests_properties(qindaqt.file-manager-viewport PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_FATAL_WARNINGS=1"
    ENVIRONMENT_MODIFICATION "DISPLAY=unset:;WAYLAND_DISPLAY=unset:;DBUS_SESSION_BUS_ADDRESS=unset:"
    LABELS "file-manager;offscreen;accessibility")

qt_add_resources(qindaqt_file_manager_browsing_ui_tests file_manager_browsing_artwork
    PREFIX /qindaqt/file-manager
    FILES "${PROJECT_SOURCE_DIR}/data/artwork/empty-folder.png")

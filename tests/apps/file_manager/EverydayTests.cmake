# SPDX-License-Identifier: GPL-3.0-or-later

# ADR-0272: the File Manager's everyday features. Included from this folder's
# CMakeLists.txt after qindaqt_add_file_manager_test is defined.

# The Recents place: reading a fixture recently-used store (order, bounds,
# refusals) and browsing it through NavigationController's lister seam.
qindaqt_add_file_manager_test(
    qindaqt_file_manager_recents_place_tests
    tst_recents_place.cpp
    qindaqt.file-manager-recents-place
)

# Production Main.qml: Quick Look from Space, Ctrl+Y and the catalog in every
# view, type-to-select in every view, and the Recents place in the window.
qindaqt_add_file_manager_test(qindaqt_file_manager_everyday_ui_tests
    tst_everyday_ui.cpp qindaqt.file-manager-everyday-ui)
target_link_libraries(qindaqt_file_manager_everyday_ui_tests PRIVATE Qt6::Quick)
target_compile_definitions(qindaqt_file_manager_everyday_ui_tests PRIVATE
    QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
set_tests_properties(qindaqt.file-manager-everyday-ui PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_FATAL_WARNINGS=1;QT_QPA_PLATFORMTHEME=generic;QT_QUICK_CONTROLS_STYLE=Fusion"
    ENVIRONMENT_MODIFICATION "DISPLAY=unset:;WAYLAND_DISPLAY=unset:;DBUS_SESSION_BUS_ADDRESS=unset:")
qt_add_resources(qindaqt_file_manager_everyday_ui_tests file_manager_everyday_ui_artwork
    PREFIX /qindaqt/file-manager
    FILES "${PROJECT_SOURCE_DIR}/data/artwork/empty-folder.png")

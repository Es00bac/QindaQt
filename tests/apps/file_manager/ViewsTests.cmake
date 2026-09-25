# SPDX-License-Identifier: GPL-3.0-or-later

# ADR-0270: the File Manager's four QindaTK views. Included from this folder's
# CMakeLists.txt after qindaqt_add_file_manager_test and the viewport QML test
# runner are defined.

# The Details view's extra sort columns and Group By, as ListingOrder policy.
qindaqt_add_file_manager_test(
    qindaqt_file_manager_listing_groups_tests
    tst_listing_groups.cpp
    qindaqt.file-manager-listing-groups
)
# Facts read for visible rows only (EntryFacts) and the Columns view's other
# columns (ColumnListing). Guiless: answers come back through the event loop.
qindaqt_add_file_manager_test(
    qindaqt_file_manager_entry_facts_tests
    tst_entry_facts.cpp
    qindaqt.file-manager-entry-facts
)

# Details headings and column edits, Columns levels and the Gallery strip,
# over a fixture navigation (the viewport runner registers theme icons).
add_test(NAME qindaqt.file-manager-views-qml
    COMMAND qindaqt_file_manager_viewport_tests
        -input "${CMAKE_CURRENT_SOURCE_DIR}/qml/tst_views.qml")
set_tests_properties(qindaqt.file-manager-views-qml PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_FATAL_WARNINGS=1;QT_QUICK_CONTROLS_STYLE=Fusion"
    ENVIRONMENT_MODIFICATION "DISPLAY=unset:;WAYLAND_DISPLAY=unset:;DBUS_SESSION_BUS_ADDRESS=unset:"
    LABELS "file-manager;offscreen;accessibility")

# Production Main.qml: selection, keyboard and context menu in every view,
# the switcher and View menu, Details sorting, grouping and columns,
# per-folder views and Use as Defaults, and the Columns view's arrows.
qindaqt_add_file_manager_test(qindaqt_file_manager_views_ui_tests
    tst_views_ui.cpp qindaqt.file-manager-views-ui)
target_link_libraries(qindaqt_file_manager_views_ui_tests PRIVATE Qt6::Quick)
target_compile_definitions(qindaqt_file_manager_views_ui_tests PRIVATE
    QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
set_tests_properties(qindaqt.file-manager-views-ui PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_FATAL_WARNINGS=1;QT_QPA_PLATFORMTHEME=generic;QT_QUICK_CONTROLS_STYLE=Fusion"
    ENVIRONMENT_MODIFICATION "DISPLAY=unset:;WAYLAND_DISPLAY=unset:;DBUS_SESSION_BUS_ADDRESS=unset:")
qt_add_resources(qindaqt_file_manager_views_ui_tests file_manager_views_ui_artwork
    PREFIX /qindaqt/file-manager
    FILES "${PROJECT_SOURCE_DIR}/data/artwork/empty-folder.png")

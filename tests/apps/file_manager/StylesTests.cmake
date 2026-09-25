# SPDX-License-Identifier: GPL-3.0-or-later

# ADR-0271: File manager styles and tabs. Included from this folder's
# CMakeLists.txt after qindaqt_add_file_manager_test is defined.

# A controller per tab, and the window's action states following the one in
# front.
qindaqt_add_file_manager_test(qindaqt_file_manager_folder_navigations_tests
    tst_folder_navigations.cpp qindaqt.file-manager-folder-navigations)
set_tests_properties(qindaqt.file-manager-folder-navigations PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_FATAL_WARNINGS=1"
    ENVIRONMENT_MODIFICATION "DISPLAY=unset:;WAYLAND_DISPLAY=unset:")

# The layout profiles' workflow.fileManager hints and Settings1's selected
# layout, over a scripted transport; no bus is reached.
qindaqt_add_file_manager_test(qindaqt_file_manager_layout_style_hint_tests
    tst_layout_style_hint.cpp qindaqt.file-manager-layout-style-hint)
target_link_libraries(qindaqt_file_manager_layout_style_hint_tests PRIVATE
    QindaQt::SettingsClient)
target_compile_definitions(qindaqt_file_manager_layout_style_hint_tests PRIVATE
    QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
set_tests_properties(qindaqt.file-manager-layout-style-hint PROPERTIES
    ENVIRONMENT "QT_FATAL_WARNINGS=1"
    ENVIRONMENT_MODIFICATION "DBUS_SESSION_BUS_ADDRESS=unset:")

# Production Main.qml: each style's chrome and starting view, tabs, the
# Commander panes and keys, and the Explorer folder tree.
qindaqt_add_file_manager_test(qindaqt_file_manager_styles_ui_tests
    tst_styles_ui.cpp qindaqt.file-manager-styles-ui)
target_link_libraries(qindaqt_file_manager_styles_ui_tests PRIVATE Qt6::Quick)
target_compile_definitions(qindaqt_file_manager_styles_ui_tests PRIVATE
    QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
set_tests_properties(qindaqt.file-manager-styles-ui PROPERTIES
    LABELS "file-manager;offscreen;accessibility"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_FATAL_WARNINGS=1;QT_QPA_PLATFORMTHEME=generic;QT_QUICK_CONTROLS_STYLE=Fusion"
    ENVIRONMENT_MODIFICATION "DISPLAY=unset:;WAYLAND_DISPLAY=unset:;DBUS_SESSION_BUS_ADDRESS=unset:")
qt_add_resources(qindaqt_file_manager_styles_ui_tests file_manager_styles_ui_artwork
    PREFIX /qindaqt/file-manager
    FILES "${PROJECT_SOURCE_DIR}/data/artwork/empty-folder.png")

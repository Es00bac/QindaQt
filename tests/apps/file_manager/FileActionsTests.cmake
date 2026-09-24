# SPDX-License-Identifier: GPL-3.0-or-later

# ADR-0269: the File Manager's right-click set. Included from this folder's
# CMakeLists.txt after qindaqt_add_file_manager_test is defined.

# ADR-0269: Open With's bounded launch and its use of Settings' association
# store, over a fixed catalog and a recording starter (nothing is spawned).
qindaqt_add_file_manager_test(
    qindaqt_file_manager_open_with_tests
    tst_open_with.cpp
    qindaqt.file-manager-open-with
)
# ADR-0269: the right-click set's file operations and the KArchive jobs on
# temporary files; the fixtures build their archives with KArchive directly.
find_package(KF6Archive 6.0 REQUIRED CONFIG)
qindaqt_add_file_manager_test(
    qindaqt_file_manager_file_actions_mutation_tests
    tst_file_actions_mutation.cpp
    qindaqt.file-manager-file-actions-mutation
)
target_link_libraries(qindaqt_file_manager_file_actions_mutation_tests PRIVATE KF6::Archive)

# ADR-0269: production Main.qml with the right-click set -- the context menu's
# visibility rules for files, folders, archives, the background and Trash,
# and each action from the catalog to its controller, over temporary files
# and a recording starter.
qindaqt_add_file_manager_test(qindaqt_file_manager_file_actions_ui_tests
    tst_file_actions_ui.cpp qindaqt.file-manager-file-actions-ui)
target_link_libraries(qindaqt_file_manager_file_actions_ui_tests PRIVATE Qt6::Quick)
target_compile_definitions(qindaqt_file_manager_file_actions_ui_tests PRIVATE
    QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
set_tests_properties(qindaqt.file-manager-file-actions-ui PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_FATAL_WARNINGS=1;QT_QPA_PLATFORMTHEME=generic;QT_QUICK_CONTROLS_STYLE=Fusion"
    ENVIRONMENT_MODIFICATION "DISPLAY=unset:;WAYLAND_DISPLAY=unset:;DBUS_SESSION_BUS_ADDRESS=unset:")
qt_add_resources(qindaqt_file_manager_file_actions_ui_tests file_manager_file_actions_ui_artwork
    PREFIX /qindaqt/file-manager
    FILES "${PROJECT_SOURCE_DIR}/data/artwork/empty-folder.png")

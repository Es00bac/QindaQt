# SPDX-License-Identifier: GPL-3.0-or-later
#
# Included from tests/shell/task_list/CMakeLists.txt (same directory scope).
# ADR-0396: per-tile popups are created on demand and release their windows
# when they close. Fatal warnings stay on: opening and closing these popups must
# render warning-free offscreen.
qindaqt_add_task_list_applet_test(
    qindaqt_task_list_popup_windows_tests
    tst_task_list_popup_windows.cpp
    qindaqt.task-list-popup-windows
    "shell;task-list;offscreen"
    task_list_applet_test_fakes.h
    task_list_operation_test_support.h
)
target_link_libraries(
    qindaqt_task_list_popup_windows_tests
    PRIVATE
        QindaQt::TokensQml
        qindaqt_shell_task_list_appletplugin
        QindaQt::ShellIcons
        qindaqt_shell_iconsplugin
        qindaqt_themes
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
)
target_compile_definitions(
    qindaqt_task_list_popup_windows_tests
    PRIVATE
        QINDAQT_TASK_LIST_APPLET_QML_IMPORT_PATH="${CMAKE_BINARY_DIR}/qml"
        QINDAQT_SOURCE_DIR="${task_list_boundary_source_root}"
)
add_dependencies(
    qindaqt_task_list_popup_windows_tests
    qindaqt_tokens_qmlplugin
    qindaqt_controls_qmlplugin
    qindaqt_shell_iconsplugin
)
qt_import_qml_plugins(qindaqt_task_list_popup_windows_tests)
set_tests_properties(
    qindaqt.task-list-popup-windows
    PROPERTIES
        ENVIRONMENT
            "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_FATAL_WARNINGS=1;QML2_IMPORT_PATH=${CMAKE_BINARY_DIR}/qml"
        TIMEOUT 300
)

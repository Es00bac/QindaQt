# SPDX-License-Identifier: GPL-3.0-or-later

find_package(Python3 REQUIRED COMPONENTS Interpreter)

function(qindaqt_add_editor_test target source test_name)
    qt_add_executable(${target} ${source})
    target_link_libraries(${target} PRIVATE qindaqt_editor_support Qt6::Test)
    target_compile_features(${target} PRIVATE cxx_std_20)
    set_target_properties(${target} PROPERTIES CXX_EXTENSIONS OFF)
    qindaqt_enable_warnings(${target})
    add_test(NAME ${test_name} COMMAND ${target})
endfunction()

qindaqt_add_editor_test(
    qindaqt_editor_document_state_tests
    tst_document_state.cpp
    qindaqt.editor-document-state
)
qindaqt_add_editor_test(
    qindaqt_editor_local_store_tests
    tst_local_document_store.cpp
    qindaqt.editor-local-store
)
qindaqt_add_editor_test(
    qindaqt_editor_controller_tests
    tst_document_controller.cpp
    qindaqt.editor-controller
)
qindaqt_add_editor_test(
    qindaqt_editor_large_document_tests
    tst_large_document.cpp
    qindaqt.editor-large-document
)
set_tests_properties(
    qindaqt.editor-large-document
    PROPERTIES LABELS "performance;editor" RUN_SERIAL TRUE TIMEOUT 30
)
qindaqt_add_editor_test(
    qindaqt_editor_window_tests
    tst_editor_window.cpp
    qindaqt.editor-window-offscreen
)
target_compile_definitions(
    qindaqt_editor_window_tests PRIVATE QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)
set_tests_properties(
    qindaqt.editor-window-offscreen
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

qindaqt_add_editor_test(
    qindaqt_editor_app_shell_tests
    tst_editor_app_shell.cpp
    qindaqt.editor-app-shell-offscreen
)
target_compile_definitions(
    qindaqt_editor_app_shell_tests PRIVATE QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)
set_tests_properties(
    qindaqt.editor-app-shell-offscreen
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

add_test(
    NAME qindaqt.editor-desktop-metadata
    COMMAND "${CMAKE_COMMAND}"
            "-DDESKTOP_FILE=${PROJECT_SOURCE_DIR}/src/apps/text_editor/org.qindaqt.TextEditor.desktop"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/check_desktop_metadata.cmake"
)
add_test(
    NAME qindaqt.editor-cli-rejects-multiple-paths
    COMMAND "${CMAKE_COMMAND}"
            "-DEDITOR_EXECUTABLE=$<TARGET_FILE:qindaqt-editor>"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/check_cli_rejection.cmake"
)
set_tests_properties(
    qindaqt.editor-cli-rejects-multiple-paths
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

# AGENT-CONTRACT: This row resolves themes and metadata only from a clean staged
# prefix; build-tree RPATHs or source data cannot satisfy the acceptance gate.
add_test(
    NAME qindaqt.editor-installed-theme-and-metadata
    COMMAND "${CMAKE_COMMAND}"
            "-DBUILD_DIRECTORY=${CMAKE_BINARY_DIR}"
            "-DINSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/installed-editor-stage"
            "-DINSTALL_BINDIR=${CMAKE_INSTALL_BINDIR}"
            "-DINSTALL_DATADIR=${CMAKE_INSTALL_DATADIR}"
            "-DCONFIGURATION=$<CONFIG>"
            "-DPYTHON_EXECUTABLE=${Python3_EXECUTABLE}"
            "-DRUNTIME_PROBE=${CMAKE_CURRENT_SOURCE_DIR}/installed_runtime_probe.py"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_installed_editor.cmake"
)
set_tests_properties(
    qindaqt.editor-installed-theme-and-metadata
    PROPERTIES
        LABELS "package;editor"
        RUN_SERIAL TRUE
        TIMEOUT 240
)

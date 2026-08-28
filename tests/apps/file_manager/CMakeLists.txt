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

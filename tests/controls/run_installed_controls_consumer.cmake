# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required IN ITEMS QINDAQT_CMAKE QINDAQT_BUILD_DIRECTORY
                          QINDAQT_INSTALL_PREFIX QINDAQT_QML_INSTALL_DIR
                          QINDAQT_QMLTESTRUNNER QINDAQT_QMLLINT
                          QINDAQT_QT_QML_IMPORT_ROOT QINDAQT_CONSUMER_QML
                          QINDAQT_EXPECTED_QML_DEPLOY_PATHS)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "Missing installed-controls input: ${required}")
    endif()
endforeach()

cmake_path(NORMAL_PATH QINDAQT_BUILD_DIRECTORY OUTPUT_VARIABLE build_directory)
cmake_path(NORMAL_PATH QINDAQT_INSTALL_PREFIX OUTPUT_VARIABLE install_prefix)
cmake_path(IS_PREFIX build_directory "${install_prefix}" NORMALIZE prefix_is_in_build)
if(NOT prefix_is_in_build OR install_prefix STREQUAL build_directory)
    message(FATAL_ERROR "Refusing to replace a staged prefix outside the test build tree")
endif()

# AGENT-GUARD: A clean stage prevents removed QML/plugin install rules from
# passing through stale payload. The prefix guard above confines deletion to
# this test's build directory.
file(REMOVE_RECURSE "${install_prefix}")
set(install_command "${QINDAQT_CMAKE}" --install "${build_directory}" --prefix "${install_prefix}")
if(DEFINED QINDAQT_CONFIGURATION AND NOT QINDAQT_CONFIGURATION STREQUAL "")
    list(APPEND install_command --config "${QINDAQT_CONFIGURATION}")
endif()
execute_process(
    COMMAND ${install_command}
    RESULT_VARIABLE install_status
    OUTPUT_VARIABLE install_output
    ERROR_VARIABLE install_error
)
if(NOT install_status EQUAL 0)
    message(FATAL_ERROR "Staged QindaQt install failed:\n${install_output}${install_error}")
endif()

set(import_root "${install_prefix}/${QINDAQT_QML_INSTALL_DIR}")
set(controls_root "${import_root}/QindaQt/Controls")
foreach(required_file IN ITEMS
        "${import_root}/QindaQt/Controls/qmldir"
        "${import_root}/QindaQt/Controls/qindaqt_controls.qmltypes"
        "${import_root}/QindaQt/Tokens/qmldir")
    if(NOT EXISTS "${required_file}")
        message(FATAL_ERROR "Installed QML payload is missing: ${required_file}")
    endif()
endforeach()

# Compare the exact staged inventory with the paths queried from Qt's module
# target. This witnesses both missing source deployment and unintended extras.
string(REPLACE "|" ";" expected_qml_paths "${QINDAQT_EXPECTED_QML_DEPLOY_PATHS}")
list(SORT expected_qml_paths)
list(LENGTH expected_qml_paths expected_qml_count)
if(NOT expected_qml_count EQUAL 14)
    message(FATAL_ERROR "Expected exactly 14 generated Controls QML paths, got ${expected_qml_count}")
endif()

file(GLOB_RECURSE installed_qml_paths
    RELATIVE "${controls_root}"
    "${controls_root}/*.qml"
)
list(SORT installed_qml_paths)
if(NOT installed_qml_paths STREQUAL expected_qml_paths)
    message(FATAL_ERROR
        "Installed Controls QML inventory does not match the generated paths:\n"
        "expected=${expected_qml_paths}\ninstalled=${installed_qml_paths}"
    )
endif()

set(staged_consumer_directory "${install_prefix}/tooling-consumer")
file(MAKE_DIRECTORY "${staged_consumer_directory}")
set(staged_consumer "${staged_consumer_directory}/tst_installed_controls.qml")
file(COPY_FILE "${QINDAQT_CONSUMER_QML}" "${staged_consumer}" ONLY_IF_DIFFERENT)

cmake_path(NORMAL_PATH QINDAQT_QT_QML_IMPORT_ROOT OUTPUT_VARIABLE qt_qml_import_root)
execute_process(
    COMMAND
        "${CMAKE_COMMAND}" -E env
        --unset=QML_IMPORT_PATH
        --unset=QML2_IMPORT_PATH
        "${QINDAQT_QMLLINT}"
        --ignore-settings
        --bare
        --max-warnings 0
        -I "${import_root}"
        -I "${qt_qml_import_root}"
        "${staged_consumer}"
    WORKING_DIRECTORY "${staged_consumer_directory}"
    RESULT_VARIABLE tooling_status
    OUTPUT_VARIABLE tooling_output
    ERROR_VARIABLE tooling_error
)
if(NOT tooling_status EQUAL 0)
    message(FATAL_ERROR
        "Installed Controls tooling consumer exited ${tooling_status}:\n"
        "${tooling_output}${tooling_error}"
    )
endif()

execute_process(
    COMMAND
        "${CMAKE_COMMAND}" -E env
        --unset=QML_IMPORT_PATH
        --unset=QML2_IMPORT_PATH
        QT_QPA_PLATFORM=offscreen
        QT_QUICK_BACKEND=software
        QML_DISABLE_DISK_CACHE=1
        "${QINDAQT_QMLTESTRUNNER}"
        -import "${import_root}"
        -input "${staged_consumer}"
    WORKING_DIRECTORY "${staged_consumer_directory}"
    RESULT_VARIABLE consumer_status
    OUTPUT_VARIABLE consumer_output
    ERROR_VARIABLE consumer_error
)
if(NOT consumer_status EQUAL 0)
    message(FATAL_ERROR
        "Installed Controls consumer exited ${consumer_status}:\n"
        "${consumer_output}${consumer_error}"
    )
endif()
message(STATUS
    "Installed QindaQt.Controls exact ${expected_qml_count}-file inventory, "
    "tooling consumer, and runtime import passed"
)

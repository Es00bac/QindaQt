# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required IN ITEMS QINDAQT_CMAKE QINDAQT_BUILD_DIRECTORY
                          QINDAQT_INSTALL_PREFIX QINDAQT_QML_INSTALL_DIR
                          QINDAQT_QMLTESTRUNNER QINDAQT_CONSUMER_QML)
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
foreach(required_file IN ITEMS
        "${import_root}/QindaQt/Controls/qmldir"
        "${import_root}/QindaQt/Controls/qindaqt_controls.qmltypes"
        "${import_root}/QindaQt/Tokens/qmldir")
    if(NOT EXISTS "${required_file}")
        message(FATAL_ERROR "Installed QML payload is missing: ${required_file}")
    endif()
endforeach()

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
        -input "${QINDAQT_CONSUMER_QML}"
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
message(STATUS "Installed QindaQt.Controls import consumer passed")

# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required IN ITEMS QINDAQT_CMAKE QINDAQT_BUILD_DIRECTORY QINDAQT_INSTALL_PREFIX
                          QINDAQT_CONSUMER_SOURCE_DIRECTORY QINDAQT_CONSUMER_SOURCE
                          QINDAQT_INSTALL_LIBDIR QINDAQT_INSTALL_INCLUDEDIR
                          QINDAQT_INSTALL_DATADIR QINDAQT_DESIGN_TOKENS_LIBRARY_NAME
                          QINDAQT_THEMES_LIBRARY_NAME QINDAQT_QT6_DIR)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "Missing installed-consumer test input: ${required}")
    endif()
endforeach()

cmake_path(NORMAL_PATH QINDAQT_BUILD_DIRECTORY OUTPUT_VARIABLE build_directory)
cmake_path(NORMAL_PATH QINDAQT_INSTALL_PREFIX OUTPUT_VARIABLE install_prefix)
cmake_path(IS_PREFIX build_directory "${install_prefix}" NORMALIZE prefix_is_in_build)
if(NOT prefix_is_in_build OR install_prefix STREQUAL build_directory)
    message(FATAL_ERROR "Refusing to replace a staged prefix outside the test build tree")
endif()

# AGENT-GUARD: Removing the prior stage is required so a deleted install rule
# cannot pass by leaving stale headers or libraries. The prefix guard above
# confines this destructive operation to the current CMake build tree.
file(REMOVE_RECURSE "${install_prefix}")

set(install_command
    "${QINDAQT_CMAKE}" --install "${build_directory}" --prefix "${install_prefix}"
)
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

set(consumer_build "${install_prefix}/consumer-build")
execute_process(
    COMMAND
        "${QINDAQT_CMAKE}"
        -S "${QINDAQT_CONSUMER_SOURCE_DIRECTORY}"
        -B "${consumer_build}"
        -G "${QINDAQT_GENERATOR}"
        "-DCMAKE_BUILD_TYPE=${QINDAQT_BUILD_TYPE}"
        "-DQt6_DIR=${QINDAQT_QT6_DIR}"
        "-DQINDAQT_STAGE_INCLUDE_DIR=${install_prefix}/${QINDAQT_INSTALL_INCLUDEDIR}"
        "-DQINDAQT_DESIGN_TOKENS_LIBRARY=${install_prefix}/${QINDAQT_INSTALL_LIBDIR}/${QINDAQT_DESIGN_TOKENS_LIBRARY_NAME}"
        "-DQINDAQT_THEMES_LIBRARY=${install_prefix}/${QINDAQT_INSTALL_LIBDIR}/${QINDAQT_THEMES_LIBRARY_NAME}"
        "-DQINDAQT_CONSUMER_SOURCE=${QINDAQT_CONSUMER_SOURCE}"
    RESULT_VARIABLE configure_status
    OUTPUT_VARIABLE configure_output
    ERROR_VARIABLE configure_error
)
if(NOT configure_status EQUAL 0)
    message(FATAL_ERROR "Installed consumer configure failed:\n${configure_output}${configure_error}")
endif()

execute_process(
    COMMAND "${QINDAQT_CMAKE}" --build "${consumer_build}" --parallel 2
    RESULT_VARIABLE build_status
    OUTPUT_VARIABLE build_output
    ERROR_VARIABLE build_error
)
if(NOT build_status EQUAL 0)
    message(FATAL_ERROR "Installed consumer build failed:\n${build_output}${build_error}")
endif()

set(consumer "${consumer_build}/qindaqt_installed_design_token_consumer")
set(theme "${install_prefix}/${QINDAQT_INSTALL_DATADIR}/qindaqt/themes/qinda-macos.json")
execute_process(
    COMMAND "${consumer}" "${theme}"
    RESULT_VARIABLE consumer_status
    OUTPUT_VARIABLE consumer_output
    ERROR_VARIABLE consumer_error
)
if(NOT consumer_status EQUAL 0)
    message(FATAL_ERROR
        "Installed C++ QST-1 consumer exited ${consumer_status}:\n"
        "${consumer_output}${consumer_error}"
    )
endif()

# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required IN ITEMS QINDAQT_CMAKE QINDAQT_BUILD_DIRECTORY
                          QINDAQT_INSTALL_PREFIX QINDAQT_INSTALL_INCLUDEDIR
                          QINDAQT_SOURCE_ROOT QINDAQT_CONSUMER_SOURCE_DIRECTORY
                          QINDAQT_QT6_DIR QINDAQT_GENERATOR CHECK_SCRIPT)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "Missing installed Display journal input: ${required}")
    endif()
endforeach()

cmake_path(NORMAL_PATH QINDAQT_BUILD_DIRECTORY OUTPUT_VARIABLE build_directory)
cmake_path(NORMAL_PATH QINDAQT_INSTALL_PREFIX OUTPUT_VARIABLE install_prefix)
cmake_path(IS_PREFIX build_directory "${install_prefix}" NORMALIZE prefix_is_in_build)
if(NOT prefix_is_in_build OR install_prefix STREQUAL build_directory)
    message(FATAL_ERROR "Refusing to replace a Display journal stage outside its build")
endif()

file(REMOVE_RECURSE "${install_prefix}")
set(install_command
    "${QINDAQT_CMAKE}" --install "${build_directory}"
    --prefix "${install_prefix}" --component DisplayJournalDevelopment
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
    message(FATAL_ERROR
        "Staged Display journal install failed:\n${install_output}${install_error}")
endif()

set(stage_include "${install_prefix}/${QINDAQT_INSTALL_INCLUDEDIR}")
execute_process(
    COMMAND "${QINDAQT_CMAKE}" "-DSTAGE_INCLUDE_DIR=${stage_include}"
            -P "${CHECK_SCRIPT}"
    RESULT_VARIABLE check_status
    OUTPUT_VARIABLE check_output
    ERROR_VARIABLE check_error
)
if(NOT check_status EQUAL 0)
    message(FATAL_ERROR
        "Installed Display journal boundary failed:\n${check_output}${check_error}")
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
        "-DQINDAQT_STAGE_INCLUDE_DIR=${stage_include}"
        "-DQINDAQT_SOURCE_ROOT=${QINDAQT_SOURCE_ROOT}"
    RESULT_VARIABLE configure_status
    OUTPUT_VARIABLE configure_output
    ERROR_VARIABLE configure_error
)
if(NOT configure_status EQUAL 0)
    message(FATAL_ERROR
        "Installed Display journal consumer configure failed:\n"
        "${configure_output}${configure_error}")
endif()
execute_process(
    COMMAND "${QINDAQT_CMAKE}" --build "${consumer_build}" --parallel 2
    RESULT_VARIABLE build_status
    OUTPUT_VARIABLE build_output
    ERROR_VARIABLE build_error
)
if(NOT build_status EQUAL 0)
    message(FATAL_ERROR
        "Installed Display journal consumer build failed:\n"
        "${build_output}${build_error}")
endif()

file(WRITE "${stage_include}/qindaqt/services/display_journal/private_file_ops.h"
     "#include <fcntl.h>\n")
execute_process(
    COMMAND "${QINDAQT_CMAKE}" "-DSTAGE_INCLUDE_DIR=${stage_include}"
            -P "${CHECK_SCRIPT}"
    RESULT_VARIABLE poison_status
    OUTPUT_VARIABLE poison_output
    ERROR_VARIABLE poison_error
)
if(poison_status EQUAL 0)
    message(FATAL_ERROR "Installed checker accepted a planted private header")
endif()

message(STATUS
    "Installed Display journal consumer, public/private boundary, and poison verified")
file(REMOVE_RECURSE "${install_prefix}")

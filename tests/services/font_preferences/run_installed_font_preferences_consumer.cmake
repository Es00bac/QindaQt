# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required IN ITEMS QINDAQT_CMAKE QINDAQT_BUILD_DIRECTORY QINDAQT_INSTALL_PREFIX
                          QINDAQT_CONSUMER_SOURCE_DIRECTORY QINDAQT_CONSUMER_SOURCE
                          QINDAQT_FONT_PREFERENCES_SOURCE_DIRECTORY
                          QINDAQT_INSTALL_INCLUDEDIR
                          QINDAQT_FONT_PREFERENCES_LIBRARY
                          QINDAQT_QT6_DIR)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "Missing installed FontPreferences consumer input: ${required}")
    endif()
endforeach()

cmake_path(NORMAL_PATH QINDAQT_BUILD_DIRECTORY OUTPUT_VARIABLE build_directory)
cmake_path(NORMAL_PATH QINDAQT_INSTALL_PREFIX OUTPUT_VARIABLE install_prefix)
cmake_path(IS_PREFIX build_directory "${install_prefix}" NORMALIZE prefix_is_in_build)
if(NOT prefix_is_in_build OR install_prefix STREQUAL build_directory)
    message(FATAL_ERROR "Refusing to replace a FontPreferences stage outside the test build tree")
endif()

file(REMOVE_RECURSE "${install_prefix}")

file(
    COPY "${QINDAQT_FONT_PREFERENCES_SOURCE_DIRECTORY}/include/"
    DESTINATION "${install_prefix}/${QINDAQT_INSTALL_INCLUDEDIR}"
)

foreach(relative IN ITEMS
        "qindaqt/services/font_preferences/font_types.h"
        "qindaqt/services/font_preferences/font_fact.h"
        "qindaqt/services/font_preferences/font_catalog.h"
        "qindaqt/services/font_preferences/font_validation.h"
        "qindaqt/services/font_preferences/font_preferences.h"
        "qindaqt/services/font_preferences/font_preferences_codec.h"
        "qindaqt/services/font_preferences/font_bootstrap.h"
        "qindaqt/services/font_preferences/font_preferences_coordinator.h")
    if(NOT EXISTS "${install_prefix}/${QINDAQT_INSTALL_INCLUDEDIR}/${relative}")
        message(FATAL_ERROR "Installed FontPreferences public header is missing ${relative}")
    endif()
endforeach()

set(consumer_build "${install_prefix}/consumer-build")
execute_process(
    COMMAND
        "${QINDAQT_CMAKE}"
        -S "${QINDAQT_CONSUMER_SOURCE_DIRECTORY}"
        -B "${consumer_build}"
        "-DCMAKE_BUILD_TYPE=${QINDAQT_BUILD_TYPE}"
        "-DQt6_DIR=${QINDAQT_QT6_DIR}"
        "-DQINDAQT_STAGE_INCLUDE_DIR=${install_prefix}/${QINDAQT_INSTALL_INCLUDEDIR}"
        "-DQINDAQT_FONT_PREFERENCES_LIBRARY=${QINDAQT_FONT_PREFERENCES_LIBRARY}"
        "-DQINDAQT_CONSUMER_SOURCE=${QINDAQT_CONSUMER_SOURCE}"
    RESULT_VARIABLE configure_status
    OUTPUT_VARIABLE configure_output
    ERROR_VARIABLE configure_error
)
if(NOT configure_status EQUAL 0)
    message(FATAL_ERROR "Installed FontPreferences consumer configure failed:\n${configure_output}${configure_error}")
endif()

execute_process(
    COMMAND "${QINDAQT_CMAKE}" --build "${consumer_build}" --parallel 1
    RESULT_VARIABLE build_status
    OUTPUT_VARIABLE build_output
    ERROR_VARIABLE build_error
)
if(NOT build_status EQUAL 0)
    message(FATAL_ERROR "Installed FontPreferences consumer build failed:\n${build_output}${build_error}")
endif()

execute_process(
    COMMAND
        "${QINDAQT_CMAKE}" -E env
        "QT_QPA_PLATFORM=offscreen"
        "${consumer_build}/qindaqt_installed_font_consumer"
    RESULT_VARIABLE consumer_status
    OUTPUT_VARIABLE consumer_output
    ERROR_VARIABLE consumer_error
)
if(NOT consumer_status EQUAL 0)
    message(FATAL_ERROR
        "Installed FontPreferences C++ consumer exited ${consumer_status}:\n"
        "${consumer_output}${consumer_error}")
endif()

message(STATUS "Installed FontPreferences consumer passed all checks successfully")

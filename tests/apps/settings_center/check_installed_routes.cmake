# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required IN ITEMS BUILD_DIRECTORY INSTALL_PREFIX INSTALL_BINDIR
                          INSTALL_DATADIR
                          SETTINGS_EXECUTABLE_NAME ROUTE_CHECK)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "missing installed Settings input: ${required}")
    endif()
endforeach()

cmake_path(NORMAL_PATH BUILD_DIRECTORY OUTPUT_VARIABLE build_directory)
cmake_path(NORMAL_PATH INSTALL_PREFIX OUTPUT_VARIABLE install_prefix)
cmake_path(IS_PREFIX build_directory "${install_prefix}" NORMALIZE in_build)
if(NOT in_build OR install_prefix STREQUAL build_directory)
    message(FATAL_ERROR "refusing to replace a Settings stage outside the build tree")
endif()
file(REMOVE_RECURSE "${install_prefix}")

set(install_command "${CMAKE_COMMAND}" --install "${build_directory}"
                    --prefix "${install_prefix}"
                    --component SettingsAppearanceRuntime)
if(DEFINED CONFIGURATION AND NOT CONFIGURATION STREQUAL "")
    list(APPEND install_command --config "${CONFIGURATION}")
endif()
execute_process(
    COMMAND ${install_command}
    RESULT_VARIABLE install_status
    OUTPUT_VARIABLE install_output
    ERROR_VARIABLE install_error
)
if(NOT install_status EQUAL 0)
    message(FATAL_ERROR "staged Settings install failed:\n${install_output}${install_error}")
endif()

set(SETTINGS_EXECUTABLE
    "${install_prefix}/${INSTALL_BINDIR}/${SETTINGS_EXECUTABLE_NAME}")
set(THEME_DIRECTORY "${install_prefix}/${INSTALL_DATADIR}/qindaqt/themes")
set(SANDBOX_ROOT "${install_prefix}/route-runtime")
# The installed regression must prove the executable discovers the staged
# theme catalog itself. The build-tree regression still injects its source
# fixture because no install layout exists there.
set(USE_DEFAULT_THEME_SEARCH TRUE)
include("${ROUTE_CHECK}")

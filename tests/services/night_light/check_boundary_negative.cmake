# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required IN ITEMS CHECK_SCRIPT MODULE_SOURCE_DIR STAGE_DIR)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "Missing boundary-poison test input: ${required}")
    endif()
endforeach()

cmake_path(NORMAL_PATH STAGE_DIR OUTPUT_VARIABLE stage)
cmake_path(NORMAL_PATH MODULE_SOURCE_DIR OUTPUT_VARIABLE module_root)
cmake_path(IS_PREFIX module_root "${stage}" NORMALIZE stage_inside_module)
if(stage_inside_module OR NOT IS_ABSOLUTE "${stage}")
    message(FATAL_ERROR "Poison stage must be an absolute path outside the source tree")
endif()

# AGENT-GUARD: The poison copy is deleted first so a stale stage cannot make a
# broken policy look caught; the copy is also never the real module directory.
# The copy keeps the repository-relative shape the boundary script expects: it globs
# ${SOURCE_ROOT}/src/services/<module>/..., so the stage must mirror that layout.
file(REMOVE_RECURSE "${stage}")
file(COPY "${module_root}/" DESTINATION "${stage}/src/services/night_light")

# The unstaged copy must be clean first: otherwise a later rejection could
# come from a pre-existing violation instead of the planted probe.
execute_process(
    COMMAND "${CMAKE_COMMAND}" "-DSOURCE_ROOT=${stage}" -P "${CHECK_SCRIPT}"
    RESULT_VARIABLE clean_status
    OUTPUT_VARIABLE clean_output
    ERROR_VARIABLE clean_error
)
if(NOT clean_status EQUAL 0)
    message(FATAL_ERROR "Boundary policy rejects the clean staged copy; poison proof would be vacuous:\n"
                        "${clean_output}${clean_error}")
endif()

file(GLOB staged_sources LIST_DIRECTORIES false "${stage}/src/services/night_light/src/*.cpp")
list(LENGTH staged_sources staged_source_count)
if(staged_source_count EQUAL 0)
    message(FATAL_ERROR "Poison staging found no source files to plant the probe in")
endif()
list(GET staged_sources 0 poison_target)

file(APPEND "${poison_target}"
     "\n#include <QtCore/QStandardPaths> // poison-negative probe\n")

execute_process(
    COMMAND "${CMAKE_COMMAND}" "-DSOURCE_ROOT=${stage}" -P "${CHECK_SCRIPT}"
    RESULT_VARIABLE poison_status
    OUTPUT_VARIABLE poison_output
    ERROR_VARIABLE poison_error
)
if(poison_status EQUAL 0)
    message(FATAL_ERROR
        "Boundary policy failed to reject a planted forbidden dependency:\n"
        "${poison_output}${poison_error}")
endif()

message(STATUS "Night light boundary policy correctly rejects planted forbidden dependencies")
file(REMOVE_RECURSE "${stage}")

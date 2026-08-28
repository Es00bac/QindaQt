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
file(REMOVE_RECURSE "${stage}")
file(COPY "${module_root}/" DESTINATION "${stage}")

file(APPEND "${stage}/src/color_model.cpp"
     "\n#include <QtDBus/QDBusConnection> // poison-negative probe\n")

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

message(STATUS "Display color model boundary policy correctly rejects planted forbidden dependencies")
file(REMOVE_RECURSE "${stage}")

# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required IN ITEMS CHECK_SCRIPT MODULE_SOURCE_DIR STAGE_DIR)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "Missing Display runtime poison input: ${required}")
    endif()
endforeach()

cmake_path(NORMAL_PATH STAGE_DIR OUTPUT_VARIABLE stage)
cmake_path(NORMAL_PATH MODULE_SOURCE_DIR OUTPUT_VARIABLE module_root)
cmake_path(IS_PREFIX module_root "${stage}" NORMALIZE stage_inside_module)
if(stage_inside_module OR NOT IS_ABSOLUTE "${stage}")
    message(FATAL_ERROR
        "Display runtime poison stage must be absolute and outside the source module")
endif()

file(REMOVE_RECURSE "${stage}")
file(MAKE_DIRECTORY "${stage}/src/services")
file(COPY "${module_root}" DESTINATION "${stage}/src/services")
file(APPEND
    "${stage}/src/services/display_runtime/include/qindaqt/services/display_runtime/state_root.h"
    "\n#include <qindaqt/services/session_lock_state/session_lock_state_monitor.h>\n"
)
execute_process(
    COMMAND "${CMAKE_COMMAND}" "-DSOURCE_ROOT=${stage}" -P "${CHECK_SCRIPT}"
    RESULT_VARIABLE poison_status
    OUTPUT_VARIABLE poison_output
    ERROR_VARIABLE poison_error
)
if(poison_status EQUAL 0)
    message(FATAL_ERROR
        "D6 boundary accepted a planted public platform dependency:\n"
        "${poison_output}${poison_error}")
endif()

file(REMOVE_RECURSE "${stage}")
message(STATUS "Display runtime source boundary rejects public poison")

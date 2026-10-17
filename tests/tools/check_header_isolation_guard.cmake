# SPDX-License-Identifier: GPL-3.0-or-later
# ADR-0388 guard proof. One probe includes a real public header path twice:
# with a module directory on -I the module copy wins over the guard; without it
# the guard's #error fires instead of any installed copy being used.
foreach(required COMPILER GUARD_DIR WORK_DIR HEADER)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()
if(NOT EXISTS "${GUARD_DIR}/${HEADER}")
    message(FATAL_ERROR "the guard has no stub for <${HEADER}>")
endif()
file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}/module/include")
get_filename_component(module_header "${WORK_DIR}/module/include/${HEADER}" ABSOLUTE)
file(WRITE "${module_header}" "#pragma once\ninline constexpr int qindaqtProbeModuleHeader = 1;\n")
file(WRITE "${WORK_DIR}/probe.cpp"
    "#include <${HEADER}>\nint main() { return qindaqtProbeModuleHeader - 1; }\n")

execute_process(COMMAND "${COMPILER}" -std=c++20 -fsyntax-only
        "-I${WORK_DIR}/module/include" -isystem "${GUARD_DIR}" "${WORK_DIR}/probe.cpp"
    RESULT_VARIABLE linked ERROR_VARIABLE linked_error)
if(NOT linked EQUAL 0)
    message(FATAL_ERROR "a linked module lost to the guard:\n${linked_error}")
endif()

execute_process(COMMAND "${COMPILER}" -std=c++20 -fsyntax-only
        -isystem "${GUARD_DIR}" "${WORK_DIR}/probe.cpp"
    RESULT_VARIABLE unlinked ERROR_VARIABLE unlinked_error)
if(unlinked EQUAL 0 OR NOT unlinked_error MATCHES "is not on this target's include path")
    message(FATAL_ERROR "an unlinked include was not stopped by the guard:\n${unlinked_error}")
endif()
message(STATUS "header isolation guard: linked module wins, unlinked include fails")

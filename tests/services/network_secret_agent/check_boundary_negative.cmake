# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required IN ITEMS POISON_ROOT CHECK_SCRIPT)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "missing secret-agent poison input: ${required}")
    endif()
endforeach()

file(REMOVE_RECURSE "${POISON_ROOT}")
file(MAKE_DIRECTORY "${POISON_ROOT}/src/services/network_secret_agent")
file(WRITE "${POISON_ROOT}/src/services/network_secret_agent/poison.cpp"
    "#include <qindaqt/services/network_protocol/network_types.h>\n")
execute_process(
    COMMAND "${CMAKE_COMMAND}" -DSOURCE_ROOT=${POISON_ROOT} -P ${CHECK_SCRIPT}
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
)
file(REMOVE_RECURSE "${POISON_ROOT}")
if(result EQUAL 0)
    message(FATAL_ERROR "secret-agent boundary accepted a Network1 dependency")
endif()
string(CONCAT combined "${output}" "${error}")
if(NOT combined MATCHES "crossed its confined process boundary")
    message(FATAL_ERROR "secret-agent poison failed for the wrong reason: ${combined}")
endif()
message(STATUS "Network secret-agent boundary rejected injected coupling")

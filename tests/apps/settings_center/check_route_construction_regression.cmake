# SPDX-License-Identifier: GPL-3.0-or-later
foreach(required IN ITEMS ROUTE_CHECK FAKE_EXECUTABLE THEME_DIRECTORY SANDBOX_ROOT)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "missing route regression input: ${required}")
    endif()
endforeach()
execute_process(
    COMMAND "${CMAKE_COMMAND}"
        -DSETTINGS_EXECUTABLE=${FAKE_EXECUTABLE}
        -DTHEME_DIRECTORY=${THEME_DIRECTORY}
        -DSANDBOX_ROOT=${SANDBOX_ROOT}
        -DEXPECTED_ROUTE_COUNT=2
        -DROUTE_TIMEOUT=1
        -P "${ROUTE_CHECK}"
    RESULT_VARIABLE status
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
)
if("${status}" STREQUAL "0" OR
   NOT error MATCHES "Settings beta route did not produce a completed construction witness")
    message(FATAL_ERROR
        "resident route without witness was not rejected as beta failure "
        "(${status}):\n${output}${error}")
endif()
message(STATUS "Missing active Loader witness rejected despite resident process")

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env FAKE_ROUTE_WARN=1
        "${CMAKE_COMMAND}"
        -DSETTINGS_EXECUTABLE=${FAKE_EXECUTABLE}
        -DTHEME_DIRECTORY=${THEME_DIRECTORY}
        -DSANDBOX_ROOT=${SANDBOX_ROOT}-warning
        -DEXPECTED_ROUTE_COUNT=2
        -DROUTE_TIMEOUT=1
        -P "${ROUTE_CHECK}"
    RESULT_VARIABLE warning_status
    OUTPUT_VARIABLE warning_output
    ERROR_VARIABLE warning_error
)
if("${warning_status}" STREQUAL "0" OR
   NOT warning_error MATCHES "Settings alpha route emitted an unexpected QML/Loader warning")
    message(FATAL_ERROR
        "ready witness with QML warning was not rejected "
        "(${warning_status}):\n${warning_output}${warning_error}")
endif()
message(STATUS "Unexpected QML warning rejected despite ready witness")

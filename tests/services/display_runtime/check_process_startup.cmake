# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required IN ITEMS QINDAQT_DISPLAY_SERVICE STAGE_DIR)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "Missing Display runtime process-startup input: ${required}")
    endif()
endforeach()

cmake_path(NORMAL_PATH STAGE_DIR OUTPUT_VARIABLE stage)
if(NOT IS_ABSOLUTE "${stage}" OR NOT stage MATCHES "/process-startup-stage$")
    message(FATAL_ERROR "Refusing unsafe process-startup stage: ${stage}")
endif()
file(REMOVE_RECURSE "${stage}")

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env --unset=STATE_DIRECTORY
            --unset=XDG_STATE_HOME --unset=HOME "${QINDAQT_DISPLAY_SERVICE}"
    RESULT_VARIABLE missing_status
    OUTPUT_VARIABLE missing_output
    ERROR_VARIABLE missing_error
)
if(NOT missing_status EQUAL 2)
    message(FATAL_ERROR
        "Missing-root process did not stop at selection (status ${missing_status}):\n"
        "${missing_output}${missing_error}")
endif()

file(MAKE_DIRECTORY "${stage}/unsafe")
file(CHMOD "${stage}/unsafe"
     PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                 GROUP_READ GROUP_WRITE GROUP_EXECUTE)
execute_process(
    COMMAND "${QINDAQT_DISPLAY_SERVICE}" --state-root "${stage}/unsafe"
    RESULT_VARIABLE unsafe_status
    OUTPUT_VARIABLE unsafe_output
    ERROR_VARIABLE unsafe_error
)
if(NOT unsafe_status EQUAL 3)
    message(FATAL_ERROR
        "Unsafe-root process passed D5 load (status ${unsafe_status}):\n"
        "${unsafe_output}${unsafe_error}")
endif()

file(MAKE_DIRECTORY "${stage}/malformed")
file(CHMOD "${stage}/malformed"
     PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)
file(WRITE "${stage}/malformed/display1-transaction.journal" "hostile-journal")
file(CHMOD "${stage}/malformed/display1-transaction.journal"
     PERMISSIONS OWNER_READ OWNER_WRITE)
execute_process(
    COMMAND "${QINDAQT_DISPLAY_SERVICE}" --state-root "${stage}/malformed"
    RESULT_VARIABLE malformed_status
    OUTPUT_VARIABLE malformed_output
    ERROR_VARIABLE malformed_error
)
if(NOT malformed_status EQUAL 3)
    message(FATAL_ERROR
        "Malformed journal process passed startup load (status ${malformed_status}):\n"
        "${malformed_output}${malformed_error}")
endif()

file(REMOVE_RECURSE "${stage}")
message(STATUS "Packaged Display1 rejects missing/unsafe/malformed state truth before buses")

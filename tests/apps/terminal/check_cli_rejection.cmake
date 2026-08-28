# Positional arguments are rejected: the terminal CLI is argv-only, never a
# shell string. The launcher must exit 2 with a diagnostic before opening any
# window or starting any session.
execute_process(
    COMMAND "${TERMINAL_EXECUTABLE}" "also-a-positional.txt"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
)
if(NOT result EQUAL 2)
    message(FATAL_ERROR "positional argument returned ${result}, expected 2: ${output}${error}")
endif()
if(NOT error MATCHES "unexpected positional arguments")
    message(FATAL_ERROR "positional-argument diagnostic missing: ${error}")
endif()

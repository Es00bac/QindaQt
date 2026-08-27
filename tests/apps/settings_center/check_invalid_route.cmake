execute_process(
    COMMAND "${SETTINGS_EXECUTABLE}" --page unknown
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
)
if(NOT result EQUAL 2)
    message(FATAL_ERROR "unknown route returned ${result}, expected 2: ${output}${error}")
endif()
if(NOT error MATCHES "unknown page: unknown")
    message(FATAL_ERROR "unknown route diagnostic missing: ${error}")
endif()

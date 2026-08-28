execute_process(
    COMMAND "${FILE_MANAGER_EXECUTABLE}" /first /second
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
)
if(NOT result EQUAL 2)
    message(FATAL_ERROR "multiple paths returned ${result}, expected 2: ${output}${error}")
endif()
if(NOT error MATCHES "open one folder at a time")
    message(FATAL_ERROR "multiple-path diagnostic missing: ${error}")
endif()

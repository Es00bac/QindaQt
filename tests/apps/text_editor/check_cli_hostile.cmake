set(arguments)
foreach(index RANGE 1 33)
    list(APPEND arguments "hostile-${index}.txt")
endforeach()
execute_process(
    COMMAND "${EDITOR_EXECUTABLE}" ${arguments}
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
)
if(NOT result EQUAL 2)
    message(FATAL_ERROR "hostile argv returned ${result}, expected 2: ${output}${error}")
endif()
if(NOT error MATCHES "at most 32")
    message(FATAL_ERROR "hostile argv diagnostic missing: ${error}")
endif()

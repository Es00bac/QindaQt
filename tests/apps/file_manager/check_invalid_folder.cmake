execute_process(
    # ADR-0116: --theme is a deprecated no-op; this call doubles as the
    # regression that the retained option does not break CLI validation.
    COMMAND "${FILE_MANAGER_EXECUTABLE}"
            --theme qindaqt-deliberately-missing-theme
            "${CMAKE_CURRENT_LIST_FILE}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
)
if(NOT result EQUAL 4)
    message(FATAL_ERROR "non-folder argument returned ${result}, expected 4: ${output}${error}")
endif()
if(NOT error MATCHES "is not a folder")
    message(FATAL_ERROR "non-folder diagnostic missing: ${error}")
endif()

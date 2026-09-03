file(REMOVE_RECURSE "${CLI_ROOT}")
file(MAKE_DIRECTORY "${CLI_ROOT}")
set(first "${CLI_ROOT}/first.txt")
set(second "${CLI_ROOT}/second.txt")
file(WRITE "${first}" "first")
file(WRITE "${second}" "second")
execute_process(
    COMMAND "${EDITOR_EXECUTABLE}" --theme-directory "${THEME_DIRECTORY}"
            --check-open-paths
            "${first}" "${second}" "${first}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "multiple paths returned ${result}: ${output}${error}")
endif()
string(STRIP "${output}" output)
set(expected "open-documents=2\npath=${first}\npath=${second}")
if(NOT output STREQUAL expected)
    message(FATAL_ERROR "multiple-path probe returned unexpected inventory: ${output}")
endif()

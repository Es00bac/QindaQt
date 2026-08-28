foreach(route IN ITEMS unknown Appearance ../appearance "appearance/extra")
    execute_process(
        COMMAND "${SETTINGS_EXECUTABLE}" --page "${route}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
    )
    if(NOT result EQUAL 2)
        message(FATAL_ERROR
            "hostile route '${route}' returned ${result}, expected 2: ${output}${error}")
    endif()
    string(FIND "${error}" "unknown page: ${route}" diagnostic_position)
    if(diagnostic_position EQUAL -1)
        message(FATAL_ERROR
            "hostile route '${route}' diagnostic missing: ${error}")
    endif()
endforeach()

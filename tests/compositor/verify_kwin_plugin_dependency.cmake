# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS
        QINDAQT_SOURCE_DIR
        QINDAQT_TEST_BINARY_ROOT
        QINDAQT_TEST_GENERATOR)
    if(NOT DEFINED ${required_variable})
        message(FATAL_ERROR "${required_variable} is required")
    endif()
endforeach()

function(configure_without_kwin suffix plugin_enabled expect_success)
    set(binary_dir "${QINDAQT_TEST_BINARY_ROOT}/${suffix}")
    set(command
        "${CMAKE_COMMAND}"
        --fresh
        -S "${QINDAQT_SOURCE_DIR}"
        -B "${binary_dir}"
        -G "${QINDAQT_TEST_GENERATOR}"
        -DBUILD_TESTING=OFF
        -DQINDAQT_BUILD_SHELL=OFF
        -DQINDAQT_BUILD_KWIN_PLUGIN=${plugin_enabled}
        -DCMAKE_DISABLE_FIND_PACKAGE_KWin=TRUE
    )
    if(DEFINED QINDAQT_TEST_CMAKE_PREFIX_PATH
       AND NOT "${QINDAQT_TEST_CMAKE_PREFIX_PATH}" STREQUAL "")
        list(APPEND command "-DCMAKE_PREFIX_PATH=${QINDAQT_TEST_CMAKE_PREFIX_PATH}")
    endif()

    execute_process(
        COMMAND ${command}
        RESULT_VARIABLE result
        OUTPUT_VARIABLE standard_output
        ERROR_VARIABLE standard_error
    )
    string(CONCAT log "${standard_output}" "\n" "${standard_error}")

    if(expect_success)
        if(NOT result EQUAL 0)
            message(FATAL_ERROR
                "explicitly disabling the KWin plugin did not configure successfully:\n${log}"
            )
        endif()
    elseif(result EQUAL 0)
        message(FATAL_ERROR "the requested KWin plugin silently configured without KWin")
    elseif(NOT log MATCHES "CMAKE_DISABLE_FIND_PACKAGE_KWin")
        message(FATAL_ERROR "configuration failed for an unrelated reason:\n${log}")
    endif()
endfunction()

# AGENT-CONTRACT: ON means the binary integration is present; OFF is the only
# supported bridge-only configuration.
configure_without_kwin(required-on ON FALSE)
configure_without_kwin(explicit-off OFF TRUE)

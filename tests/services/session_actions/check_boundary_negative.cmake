# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required IN ITEMS SOURCE_ROOT POISON_ROOT CHECK_SCRIPT)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "missing session-actions poison input: ${required}")
    endif()
endforeach()

function(run_boundary result_name)
    execute_process(
        COMMAND "${CMAKE_COMMAND}"
            "-DSOURCE_ROOT=${SOURCE_ROOT}"
            "-DSCAN_ROOT=${POISON_ROOT}"
            -P "${CHECK_SCRIPT}"
        RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error)
    set(${result_name} "${status}" PARENT_SCOPE)
    set(${result_name}_log "${output}${error}" PARENT_SCOPE)
endfunction()

file(REMOVE_RECURSE "${POISON_ROOT}")
file(MAKE_DIRECTORY "${POISON_ROOT}/src/apps/settings/power")
file(WRITE "${POISON_ROOT}/src/apps/settings/power/allowed.cpp"
    "#include <qindaqt/services/session_actions/session_actions_client.h>\n")
run_boundary(allowed_status)
if(NOT allowed_status EQUAL 0)
    message(FATAL_ERROR
        "session-actions boundary rejected its public-client control:\n${allowed_status_log}")
endif()

file(WRITE "${POISON_ROOT}/src/apps/settings/power/poison.cpp"
    "const char *service = \"org.freedesktop.login1\";\n")
run_boundary(poison_status)
if(poison_status EQUAL 0)
    message(FATAL_ERROR "session-actions boundary accepted direct login1 poison")
endif()
if(NOT poison_status_log MATCHES "Direct session authority escaped session_actions")
    message(FATAL_ERROR
        "session-actions poison failed for the wrong reason:\n${poison_status_log}")
endif()

file(WRITE "${POISON_ROOT}/src/apps/settings/power/poison.cpp"
    "const char *service = \"org.freedesktop.ScreenSaver\";\n")
run_boundary(poison_status)
if(poison_status EQUAL 0)
    message(FATAL_ERROR "session-actions boundary accepted direct ScreenSaver poison")
endif()
if(NOT poison_status_log MATCHES "Direct session authority escaped session_actions")
    message(FATAL_ERROR
        "session-actions ScreenSaver poison failed for the wrong reason:\n${poison_status_log}")
endif()

file(REMOVE "${POISON_ROOT}/src/apps/settings/power/poison.cpp")

file(WRITE "${POISON_ROOT}/src/apps/settings/power/qt_screen_lock_configurator.cpp"
    "const char *service = \"org.kde.screensaver\";\n"
    "const char *path = \"/ScreenSaver\";\n"
    "const char *method = \"configure\";\n")
run_boundary(allowed_status)
if(NOT allowed_status EQUAL 0)
    message(FATAL_ERROR
        "session-actions boundary rejected the ADR-0091 configurator exception:\n${allowed_status_log}")
endif()

file(APPEND "${POISON_ROOT}/src/apps/settings/power/qt_screen_lock_configurator.cpp"
    "const char *method = \"Lock\";\n")
run_boundary(poison_status)
if(poison_status EQUAL 0)
    message(FATAL_ERROR "session-actions boundary accepted screen-lock action poison")
endif()
if(NOT poison_status_log MATCHES "configure-only authority")
    message(FATAL_ERROR
        "screen-lock action poison failed for the wrong reason:\n${poison_status_log}")
endif()

file(REMOVE_RECURSE "${POISON_ROOT}")
message(STATUS "session-actions boundary rejected direct platform-authority poison")

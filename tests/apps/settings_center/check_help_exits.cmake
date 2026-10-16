# SPDX-License-Identifier: GPL-3.0-or-later
#
# ADR-0395: --help and usage errors leave qindaqt-settings through a normal
# return from main(), not exit() from inside QCommandLineParser::process() with
# the QApplication alive. The observable contract is the status and text on the
# right stream and a completely quiet stderr otherwise: an exit with live Qt
# state is where late warnings and aborts came from.

foreach(required IN ITEMS SETTINGS_EXECUTABLE SANDBOX_ROOT)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "missing Settings help input: ${required}")
    endif()
endforeach()
file(MAKE_DIRECTORY "${SANDBOX_ROOT}/runtime")
file(CHMOD "${SANDBOX_ROOT}/runtime"
     PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)
set(sandbox_command "${CMAKE_COMMAND}" -E env
    --unset=DISPLAY
    --unset=WAYLAND_DISPLAY
    QT_QPA_PLATFORM=offscreen
    DBUS_SESSION_BUS_ADDRESS=unix:path=${SANDBOX_ROOT}/absent-session-bus
    DBUS_SYSTEM_BUS_ADDRESS=unix:path=${SANDBOX_ROOT}/absent-system-bus
    XDG_RUNTIME_DIR=${SANDBOX_ROOT}/runtime)

function(run_settings expected_status expected_stream expected_text)
    execute_process(
        COMMAND ${sandbox_command} "${SETTINGS_EXECUTABLE}" ${ARGN}
        WORKING_DIRECTORY "${SANDBOX_ROOT}"
        TIMEOUT 20
        RESULT_VARIABLE status
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
    )
    if(NOT "${status}" STREQUAL "${expected_status}")
        message(FATAL_ERROR
            "qindaqt-settings ${ARGN} exited ${status}, expected ${expected_status}:\n"
            "${output}${error}")
    endif()
    if("${expected_stream}" STREQUAL "stdout")
        set(text "${output}")
        set(quiet "${error}")
    else()
        set(text "${error}")
        set(quiet "${output}")
    endif()
    string(FIND "${text}" "${expected_text}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR
            "qindaqt-settings ${ARGN}: expected '${expected_text}' on ${expected_stream}:\n"
            "${output}${error}")
    endif()
    # Nothing but the intended text: a warning here means Qt state was
    # torn down badly on the way out.
    string(STRIP "${quiet}" quiet)
    if(NOT "${quiet}" STREQUAL "")
        message(FATAL_ERROR
            "qindaqt-settings ${ARGN}: unexpected output on the other stream:\n${quiet}")
    endif()
endfunction()

run_settings(0 stdout "Usage:" --help)
run_settings(0 stdout "--route-construction-probe" -h)
run_settings(1 stderr "Unknown option" --no-such-option)
run_settings(1 stderr "Missing value" --page)
message(STATUS "qindaqt-settings returns from main() for --help and usage errors")

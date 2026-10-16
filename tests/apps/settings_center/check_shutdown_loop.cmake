# SPDX-License-Identifier: GPL-3.0-or-later
#
# ADR-0395: twenty offscreen open/close cycles of the real qindaqt-settings.
# Settings aborted ten times between 2026-10-08 and 2026-10-16 when the QML
# scene outlived the models it reads. The route-construction probe now quits
# the event loop and lets main() run its REAL teardown, so every cycle here
# exercises the production exit path. A cycle fails on any non-zero status (a
# crash reports as "Child aborted" or a signal name), on a missing witness, or
# on any warning at all about QML, a destroyed object or the heap.
#
# MALLOC_PERTURB_ fills freed memory with a poison byte, so a read of an object
# that was already destroyed returns garbage deterministically instead of the
# stale-but-plausible value that would let it pass.

foreach(required IN ITEMS SETTINGS_EXECUTABLE THEME_DIRECTORY SANDBOX_ROOT)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "missing Settings shutdown loop input: ${required}")
    endif()
endforeach()
if(NOT DEFINED CYCLES)
    set(CYCLES 20)
endif()
if(NOT DEFINED CYCLE_TIMEOUT)
    set(CYCLE_TIMEOUT 40)
endif()
# Pages that own the heaviest model graphs and the one that was open when the
# 26-hour instance stalled.
set(pages appearance audio display network bluetooth notifications)
list(LENGTH pages page_count)

file(REMOVE_RECURSE "${SANDBOX_ROOT}")
file(MAKE_DIRECTORY "${SANDBOX_ROOT}/config" "${SANDBOX_ROOT}/data"
                    "${SANDBOX_ROOT}/system-data" "${SANDBOX_ROOT}/cache"
                    "${SANDBOX_ROOT}/state" "${SANDBOX_ROOT}/runtime")
file(CHMOD "${SANDBOX_ROOT}/runtime"
     PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)
set(sandbox_command "${CMAKE_COMMAND}" -E env
    --unset=DISPLAY
    --unset=WAYLAND_DISPLAY
    --unset=QML_IMPORT_PATH
    --unset=QML2_IMPORT_PATH
    --unset=LD_LIBRARY_PATH
    --unset=QT_PLUGIN_PATH
    --unset=QT_QPA_PLATFORM_PLUGIN_PATH
    QT_QPA_PLATFORM=offscreen
    QT_QUICK_BACKEND=software
    QML_DISABLE_DISK_CACHE=1
    MALLOC_PERTURB_=165
    DBUS_SESSION_BUS_ADDRESS=unix:path=${SANDBOX_ROOT}/absent-session-bus
    DBUS_SYSTEM_BUS_ADDRESS=unix:path=${SANDBOX_ROOT}/absent-system-bus
    XDG_CONFIG_HOME=${SANDBOX_ROOT}/config
    XDG_DATA_HOME=${SANDBOX_ROOT}/data
    XDG_DATA_DIRS=${SANDBOX_ROOT}/system-data
    XDG_CACHE_HOME=${SANDBOX_ROOT}/cache
    XDG_STATE_HOME=${SANDBOX_ROOT}/state
    XDG_RUNTIME_DIR=${SANDBOX_ROOT}/runtime)

math(EXPR last_cycle "${CYCLES} - 1")
foreach(cycle RANGE 0 ${last_cycle})
    math(EXPR index "${cycle} % ${page_count}")
    list(GET pages ${index} page)
    execute_process(
        COMMAND ${sandbox_command} "${SETTINGS_EXECUTABLE}" --page "${page}"
                --route-construction-probe --theme-directory "${THEME_DIRECTORY}"
        WORKING_DIRECTORY "${SANDBOX_ROOT}"
        TIMEOUT ${CYCLE_TIMEOUT}
        RESULT_VARIABLE status
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
    )
    math(EXPR human "${cycle} + 1")
    if(NOT "${status}" STREQUAL "0")
        message(FATAL_ERROR
            "cycle ${human}/${CYCLES} (${page}) did not close cleanly: ${status}\n"
            "${output}${error}")
    endif()
    if(NOT output MATCHES "QINDAQT_ROUTE_CONSTRUCTED ${page} (ready|diagnosed-unavailable)")
        message(FATAL_ERROR
            "cycle ${human}/${CYCLES} (${page}) opened without its construction witness:\n"
            "${output}${error}")
    endif()
    # Service-unavailability qWarning lines are expected under absent buses.
    # Anything about QML, a null or destroyed object, or the heap at close is
    # the teardown defect itself.
    if(error MATCHES
       "(\\.qml(:[0-9]+|\\.[^ ]+:[0-9]+)|QQml|ReferenceError|TypeError|Cannot read propert|Binding loop|Unable to assign|Cannot assign|Loader.*[Ee]rror|QObject::~QObject|QObject: Cannot|double free|corrupt|invalid pointer|free\\(\\))")
        message(FATAL_ERROR
            "cycle ${human}/${CYCLES} (${page}) emitted a teardown warning:\n${error}")
    endif()
endforeach()
message(STATUS "${CYCLES} open/close cycles across ${page_count} pages: zero aborts, zero teardown warnings")

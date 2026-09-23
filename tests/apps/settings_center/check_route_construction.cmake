# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required IN ITEMS SETTINGS_EXECUTABLE THEME_DIRECTORY SANDBOX_ROOT)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "missing Settings route input: ${required}")
    endif()
endforeach()
if(NOT EXISTS "${SETTINGS_EXECUTABLE}")
    message(FATAL_ERROR "Settings executable does not exist: ${SETTINGS_EXECUTABLE}")
endif()
if(NOT IS_DIRECTORY "${THEME_DIRECTORY}")
    message(FATAL_ERROR "Settings theme directory does not exist: ${THEME_DIRECTORY}")
endif()
if(NOT DEFINED EXPECTED_ROUTE_COUNT)
    set(EXPECTED_ROUTE_COUNT 21)
endif()
if(NOT DEFINED ROUTE_TIMEOUT)
    set(ROUTE_TIMEOUT 12)
endif()

file(MAKE_DIRECTORY "${SANDBOX_ROOT}/config" "${SANDBOX_ROOT}/data"
                    "${SANDBOX_ROOT}/system-data" "${SANDBOX_ROOT}/cache"
                    "${SANDBOX_ROOT}/state" "${SANDBOX_ROOT}/runtime")
file(CHMOD "${SANDBOX_ROOT}/runtime"
     PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)

# AGENT-CONTRACT: the executable's SettingsRouteRegistry is the one route
# inventory. This harness consumes it, never maintains a second route list.
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
    DBUS_SESSION_BUS_ADDRESS=unix:path=${SANDBOX_ROOT}/absent-session-bus
    DBUS_SYSTEM_BUS_ADDRESS=unix:path=${SANDBOX_ROOT}/absent-system-bus
    XDG_CONFIG_HOME=${SANDBOX_ROOT}/config
    XDG_DATA_HOME=${SANDBOX_ROOT}/data
    XDG_DATA_DIRS=${SANDBOX_ROOT}/system-data
    XDG_CACHE_HOME=${SANDBOX_ROOT}/cache
    XDG_STATE_HOME=${SANDBOX_ROOT}/state
    XDG_RUNTIME_DIR=${SANDBOX_ROOT}/runtime)

execute_process(
    COMMAND ${sandbox_command} "${SETTINGS_EXECUTABLE}" --list-routes
    WORKING_DIRECTORY "${SANDBOX_ROOT}"
    TIMEOUT ${ROUTE_TIMEOUT}
    RESULT_VARIABLE inventory_status
    OUTPUT_VARIABLE inventory
    ERROR_VARIABLE inventory_error
)
if(NOT "${inventory_status}" STREQUAL "0")
    message(FATAL_ERROR
        "Settings registry inventory failed (${inventory_status}):\n"
        "${inventory}${inventory_error}")
endif()
string(REPLACE "\n" ";" routes "${inventory}")
list(FILTER routes EXCLUDE REGEX "^$")
list(LENGTH routes route_count)
if(NOT route_count EQUAL EXPECTED_ROUTE_COUNT)
    message(FATAL_ERROR
        "Settings registry exposed ${route_count} routes, expected "
        "${EXPECTED_ROUTE_COUNT}: ${inventory}")
endif()
set(unique_routes "${routes}")
list(REMOVE_DUPLICATES unique_routes)
list(LENGTH unique_routes unique_count)
if(NOT unique_count EQUAL route_count)
    message(FATAL_ERROR "Settings registry inventory has duplicate routes: ${inventory}")
endif()

foreach(route IN LISTS routes)
    if(NOT route MATCHES "^[a-z0-9][a-z0-9_-]*$")
        message(FATAL_ERROR "Settings registry emitted invalid route ID: ${route}")
    endif()
    set(command "${SETTINGS_EXECUTABLE}" --page "${route}"
                --route-construction-probe)
    # The navigation chrome consumes QST-1; the build test injects a complete
    # theme catalog while the staged test must discover its installed catalog.
    if(NOT USE_DEFAULT_THEME_SEARCH)
        list(APPEND command --theme-directory "${THEME_DIRECTORY}")
    endif()
    execute_process(
        COMMAND ${sandbox_command} ${command}
        WORKING_DIRECTORY "${SANDBOX_ROOT}"
        TIMEOUT ${ROUTE_TIMEOUT}
        RESULT_VARIABLE status
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
    )
    if(NOT "${status}" STREQUAL "0")
        message(FATAL_ERROR
            "Settings ${route} route did not produce a completed construction "
            "witness (${status}):\n${output}${error}")
    endif()
    string(REGEX MATCHALL
        "QINDAQT_ROUTE_CONSTRUCTED [a-z0-9_-]+ (ready|diagnosed-unavailable)"
        witnesses "${output}")
    list(LENGTH witnesses witness_count)
    if(NOT witness_count EQUAL 1 OR
       NOT "${witnesses}" MATCHES
           "^QINDAQT_ROUTE_CONSTRUCTED ${route} (ready|diagnosed-unavailable)$")
        message(FATAL_ERROR
            "Settings ${route} route exited without its exact active Loader "
            "construction witness: stdout=${output} stderr=${error}")
    endif()
    # Service-unavailability qWarning lines are expected under poisoned buses.
    # QML/Loader warnings are never expected: they indicate a page binding,
    # import, required property, or component failed despite process exit 0.
    if(error MATCHES
       "(\\.qml(:[0-9]+|\\.[^ ]+:[0-9]+)|QQml|ReferenceError|TypeError|Binding loop|Unable to assign|Cannot assign|Loader.*[Ee]rror)")
        message(FATAL_ERROR
            "Settings ${route} route emitted an unexpected QML/Loader warning:\n${error}")
    endif()
    message(STATUS "Settings route ${route}: ${witnesses}")
endforeach()
message(STATUS "All ${route_count} registered Settings routes produced active Loader witnesses under isolated session/system buses")

# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required IN ITEMS SETTINGS_EXECUTABLE SANDBOX_ROOT)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "missing Settings theme-poison input: ${required}")
    endif()
endforeach()

file(MAKE_DIRECTORY "${SANDBOX_ROOT}/config" "${SANDBOX_ROOT}/data"
                    "${SANDBOX_ROOT}/system-data" "${SANDBOX_ROOT}/cache"
                    "${SANDBOX_ROOT}/runtime")
file(CHMOD "${SANDBOX_ROOT}/runtime"
     PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env
            --unset=DISPLAY
            --unset=WAYLAND_DISPLAY
            --unset=QML_IMPORT_PATH
            --unset=QML2_IMPORT_PATH
            --unset=LD_LIBRARY_PATH
            QT_QPA_PLATFORM=offscreen
            QT_QUICK_BACKEND=software
            DBUS_SESSION_BUS_ADDRESS=unix:path=${SANDBOX_ROOT}/absent-session-bus
            XDG_CONFIG_HOME=${SANDBOX_ROOT}/config
            XDG_DATA_HOME=${SANDBOX_ROOT}/data
            XDG_DATA_DIRS=${SANDBOX_ROOT}/system-data
            XDG_CACHE_HOME=${SANDBOX_ROOT}/cache
            XDG_RUNTIME_DIR=${SANDBOX_ROOT}/runtime
            "${SETTINGS_EXECUTABLE}" --page notifications
    WORKING_DIRECTORY "${SANDBOX_ROOT}"
    RESULT_VARIABLE status
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
)
if(NOT status EQUAL 3)
    message(FATAL_ERROR
        "missing-theme poison returned ${status}, expected 3: ${output}${error}")
endif()
if(NOT error MATCHES "no theme JSON files found")
    message(FATAL_ERROR "missing-theme diagnostic is not truthful: ${error}")
endif()

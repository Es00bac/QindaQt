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

file(MAKE_DIRECTORY "${SANDBOX_ROOT}/config" "${SANDBOX_ROOT}/data"
                    "${SANDBOX_ROOT}/system-data" "${SANDBOX_ROOT}/cache"
                    "${SANDBOX_ROOT}/runtime")
file(CHMOD "${SANDBOX_ROOT}/runtime"
     PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)

function(require_constructed_route route)
    set(command "${SETTINGS_EXECUTABLE}" --page "${route}")
    # The S1 navigation chrome itself consumes QST-1, so every route needs the
    # same complete theme generation before Main.qml is constructed.
    if(NOT USE_DEFAULT_THEME_SEARCH)
        list(APPEND command --theme-directory "${THEME_DIRECTORY}")
    endif()
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env
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
                XDG_CONFIG_HOME=${SANDBOX_ROOT}/config
                XDG_DATA_HOME=${SANDBOX_ROOT}/data
                XDG_DATA_DIRS=${SANDBOX_ROOT}/system-data
                XDG_CACHE_HOME=${SANDBOX_ROOT}/cache
                XDG_RUNTIME_DIR=${SANDBOX_ROOT}/runtime
                ${command}
        WORKING_DIRECTORY "${SANDBOX_ROOT}"
        TIMEOUT 3
        RESULT_VARIABLE status
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
    )
    # A constructed offscreen ApplicationWindow remains in application.exec().
    # CMake's bounded termination is therefore the success witness. Any normal
    # exit means root/module construction failed before the event loop stayed
    # resident, including a missing required property or loader dependency.
    if(NOT status MATCHES "[Tt]imeout")
        message(FATAL_ERROR
            "Settings ${route} route exited before bounded construction proof "
            "(${status}):\n${output}${error}")
    endif()
endfunction()

require_constructed_route(notifications)
require_constructed_route(appearance)
message(STATUS "Both Settings root routes remained constructed under sanitized offscreen authority loss")

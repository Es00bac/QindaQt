# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required IN ITEMS BUILD_DIRECTORY INSTALL_PREFIX INSTALL_BINDIR
                          INSTALL_DATADIR INSTALL_QMLDIR SETTINGS_EXECUTABLE_NAME)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "missing installed Clipboard route input: ${required}")
    endif()
endforeach()
cmake_path(NORMAL_PATH BUILD_DIRECTORY OUTPUT_VARIABLE build_directory)
cmake_path(NORMAL_PATH INSTALL_PREFIX OUTPUT_VARIABLE install_prefix)
cmake_path(IS_PREFIX build_directory "${install_prefix}" NORMALIZE in_build)
if(NOT in_build OR install_prefix STREQUAL build_directory)
    message(FATAL_ERROR "Clipboard route stage must remain below its build tree")
endif()
file(REMOVE_RECURSE "${install_prefix}")

set(install_command "${CMAKE_COMMAND}" --install "${build_directory}"
    --prefix "${install_prefix}" --component SettingsAppearanceRuntime)
if(DEFINED CONFIGURATION AND NOT CONFIGURATION STREQUAL "")
    list(APPEND install_command --config "${CONFIGURATION}")
endif()
execute_process(COMMAND ${install_command} RESULT_VARIABLE install_status
                OUTPUT_VARIABLE install_output ERROR_VARIABLE install_error)
if(NOT install_status EQUAL 0)
    message(FATAL_ERROR "Clipboard route install failed:\n${install_output}${install_error}")
endif()

set(executable "${install_prefix}/${INSTALL_BINDIR}/${SETTINGS_EXECUTABLE_NAME}")
set(module "${install_prefix}/${INSTALL_QMLDIR}/QindaQt/SettingsApp/Clipboard")
set(theme_directory "${install_prefix}/${INSTALL_DATADIR}/qindaqt/themes")
foreach(required_path IN ITEMS "${executable}" "${module}" "${theme_directory}")
    if(NOT EXISTS "${required_path}")
        message(FATAL_ERROR "installed Clipboard route payload missing: ${required_path}")
    endif()
endforeach()
set(sandbox "${install_prefix}/clipboard-route-runtime")
file(MAKE_DIRECTORY "${sandbox}/config" "${sandbox}/data"
                    "${sandbox}/system-data" "${sandbox}/cache" "${sandbox}/runtime")
file(CHMOD "${sandbox}/runtime" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)

function(run_route result_name)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env
                --unset=DISPLAY --unset=WAYLAND_DISPLAY
                --unset=QML_IMPORT_PATH --unset=QML2_IMPORT_PATH
                --unset=LD_LIBRARY_PATH --unset=QT_PLUGIN_PATH
                --unset=QT_QPA_PLATFORM_PLUGIN_PATH
                QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software
                QML_DISABLE_DISK_CACHE=1
                DBUS_SESSION_BUS_ADDRESS=unix:path=${sandbox}/absent-session-bus
                DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent
                XDG_CONFIG_HOME=${sandbox}/config XDG_DATA_HOME=${sandbox}/data
                XDG_DATA_DIRS=${sandbox}/system-data XDG_CACHE_HOME=${sandbox}/cache
                XDG_RUNTIME_DIR=${sandbox}/runtime
                "${executable}" --page clipboard
        WORKING_DIRECTORY "${sandbox}" TIMEOUT 3
        RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error)
    set(${result_name} "${status}" PARENT_SCOPE)
    set(${result_name}_log "${output}${error}" PARENT_SCOPE)
endfunction()

run_route(route_status)
if(NOT route_status MATCHES "[Tt]imeout")
    message(FATAL_ERROR
        "relocated Clipboard route did not remain constructed (${route_status}):\n${route_status_log}")
endif()
message(STATUS "Relocated Clipboard Settings route remained resident with host buses poisoned")

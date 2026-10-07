# SPDX-License-Identifier: GPL-3.0-or-later
foreach(required IN ITEMS BUILD_DIRECTORY INSTALL_PREFIX INSTALL_BINDIR
    INSTALL_DATADIR INSTALL_QMLDIR SETTINGS_EXECUTABLE_NAME)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "missing installed printing input: ${required}")
    endif()
endforeach()
cmake_path(NORMAL_PATH BUILD_DIRECTORY OUTPUT_VARIABLE build_directory)
cmake_path(NORMAL_PATH INSTALL_PREFIX OUTPUT_VARIABLE install_prefix)
cmake_path(IS_PREFIX build_directory "${install_prefix}" NORMALIZE in_build)
if(NOT in_build OR install_prefix STREQUAL build_directory)
    message(FATAL_ERROR "printing stage must remain below its build tree")
endif()
file(REMOVE_RECURSE "${install_prefix}")
set(command "${CMAKE_COMMAND}" --install "${build_directory}"
    --prefix "${install_prefix}" --component SettingsAppearanceRuntime)
if(DEFINED CONFIGURATION AND NOT CONFIGURATION STREQUAL "")
    list(APPEND command --config "${CONFIGURATION}")
endif()
execute_process(COMMAND ${command} RESULT_VARIABLE installed
    OUTPUT_VARIABLE install_output ERROR_VARIABLE install_error)
if(NOT installed EQUAL 0)
    message(FATAL_ERROR "printing stage install failed: ${install_output}${install_error}")
endif()

set(executable "${install_prefix}/${INSTALL_BINDIR}/${SETTINGS_EXECUTABLE_NAME}")
set(module "${install_prefix}/${INSTALL_QMLDIR}/QindaQt/SettingsApp/Printing")
cmake_path(NORMAL_PATH module)
cmake_path(IS_PREFIX install_prefix "${module}" NORMALIZE module_in_stage)
if(NOT module_in_stage OR NOT IS_DIRECTORY "${module}")
    message(FATAL_ERROR "printing module is missing or outside the ignored stage")
endif()
set(sandbox "${install_prefix}/printing-route-runtime")
file(MAKE_DIRECTORY "${sandbox}/config" "${sandbox}/data"
    "${sandbox}/system-data" "${sandbox}/cache" "${sandbox}/runtime")
file(CHMOD "${sandbox}/runtime" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)

function(run_route result_name)
    execute_process(COMMAND "${CMAKE_COMMAND}" -E env
        --unset=DISPLAY --unset=WAYLAND_DISPLAY
        --unset=QML_IMPORT_PATH --unset=QML2_IMPORT_PATH
        --unset=LD_LIBRARY_PATH --unset=QT_PLUGIN_PATH
        --unset=QT_QPA_PLATFORM_PLUGIN_PATH
        QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software
        QML_DISABLE_DISK_CACHE=1
        DBUS_SESSION_BUS_ADDRESS=unix:path=${sandbox}/absent-session-bus
        DBUS_SYSTEM_BUS_ADDRESS=unix:path=${sandbox}/absent-system-bus
        XDG_CONFIG_HOME=${sandbox}/config XDG_DATA_HOME=${sandbox}/data
        XDG_DATA_DIRS=${sandbox}/system-data XDG_CACHE_HOME=${sandbox}/cache
        XDG_RUNTIME_DIR=${sandbox}/runtime
        "${executable}" --page printers-scanners --route-construction-probe
        WORKING_DIRECTORY "${sandbox}" TIMEOUT 15
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    set(${result_name} "${result}" PARENT_SCOPE)
    set(${result_name}_output "${output}" PARENT_SCOPE)
    set(${result_name}_error "${error}" PARENT_SCOPE)
endfunction()
function(require_ready name)
    if(NOT "${${name}}" STREQUAL "0" OR
       NOT "${${name}_output}" MATCHES "QINDAQT_ROUTE_CONSTRUCTED printers-scanners ready")
        message(FATAL_ERROR "installed printing page has no exact Ready witness: ${${name}} ${${name}_output}${${name}_error}")
    endif()
    if("${${name}_error}" MATCHES "(\\.qml(:[0-9]+|\\.[^ ]+:[0-9]+)|QQml|ReferenceError|TypeError|Binding loop|Unable to assign|Cannot assign|Loader.*[Ee]rror)")
        message(FATAL_ERROR "installed printing page emitted QML warnings: ${${name}_error}")
    endif()
endfunction()
run_route(before)
require_ready(before)
# The developer module remains present. Only the installed module is withheld;
# restore before evaluation so failure cannot leave this private stage broken.
file(RENAME "${module}" "${module}.withheld")
run_route(poison)
file(RENAME "${module}.withheld" "${module}")
if(NOT "${poison}" STREQUAL "3")
    message(FATAL_ERROR "missing installed printing module borrowed another source: ${poison} ${poison_output}${poison_error}")
endif()
run_route(restored)
require_ready(restored)
message(STATUS "Installed printing page Ready, module poison refused, restored Ready; both buses and XDG roots isolated")

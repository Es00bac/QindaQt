# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required IN ITEMS BUILD_DIRECTORY INSTALL_PREFIX INSTALL_BINDIR
                          INSTALL_DATADIR INSTALL_QMLDIR
                          SETTINGS_EXECUTABLE_NAME ROUTE_CHECK)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "missing installed Settings input: ${required}")
    endif()
endforeach()

cmake_path(NORMAL_PATH BUILD_DIRECTORY OUTPUT_VARIABLE build_directory)
cmake_path(NORMAL_PATH INSTALL_PREFIX OUTPUT_VARIABLE install_prefix)
cmake_path(IS_PREFIX build_directory "${install_prefix}" NORMALIZE in_build)
if(NOT in_build OR install_prefix STREQUAL build_directory)
    message(FATAL_ERROR "refusing to replace a Settings stage outside the build tree")
endif()
file(REMOVE_RECURSE "${install_prefix}")

set(install_command "${CMAKE_COMMAND}" --install "${build_directory}"
                    --prefix "${install_prefix}"
                    --component SettingsAppearanceRuntime)
if(DEFINED CONFIGURATION AND NOT CONFIGURATION STREQUAL "")
    list(APPEND install_command --config "${CONFIGURATION}")
endif()
execute_process(
    COMMAND ${install_command}
    RESULT_VARIABLE install_status
    OUTPUT_VARIABLE install_output
    ERROR_VARIABLE install_error
)
if(NOT install_status EQUAL 0)
    message(FATAL_ERROR "staged Settings install failed:\n${install_output}${install_error}")
endif()

file(GLOB_RECURSE installed_navigation_archives
     "${install_prefix}/*qindaqt_settings_navigation*")
if(installed_navigation_archives)
    message(FATAL_ERROR
        "internal Settings navigation library leaked into runtime component: "
        "${installed_navigation_archives}")
endif()

set(SETTINGS_EXECUTABLE
    "${install_prefix}/${INSTALL_BINDIR}/${SETTINGS_EXECUTABLE_NAME}")
set(THEME_DIRECTORY "${install_prefix}/${INSTALL_DATADIR}/qindaqt/themes")

cmake_path(IS_ABSOLUTE INSTALL_QMLDIR qml_dir_is_absolute)
if(qml_dir_is_absolute)
    message(FATAL_ERROR "installed Settings QML directory must be prefix-relative")
endif()
set(appearance_module
    "${install_prefix}/${INSTALL_QMLDIR}/QindaQt/SettingsApp/Appearance")
cmake_path(NORMAL_PATH appearance_module OUTPUT_VARIABLE appearance_module)
cmake_path(IS_PREFIX install_prefix "${appearance_module}" NORMALIZE module_in_stage)
if(NOT module_in_stage OR NOT IS_DIRECTORY "${appearance_module}")
    message(FATAL_ERROR
        "installed Settings Appearance module is missing or outside stage: "
        "${appearance_module}")
endif()

set(build_appearance_module
    "${build_directory}/qml/QindaQt/SettingsApp/Appearance")
if(NOT IS_DIRECTORY "${build_appearance_module}")
    message(FATAL_ERROR
        "package poison requires the developer Appearance QML tree to remain present")
endif()

set(withheld_module "${appearance_module}.withheld")
set(poison_sandbox "${install_prefix}/package-poison")
file(MAKE_DIRECTORY "${poison_sandbox}/config" "${poison_sandbox}/data"
                    "${poison_sandbox}/system-data" "${poison_sandbox}/cache"
                    "${poison_sandbox}/runtime")
file(CHMOD "${poison_sandbox}/runtime"
     PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)
file(RENAME "${appearance_module}" "${withheld_module}")
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
            DBUS_SESSION_BUS_ADDRESS=unix:path=${poison_sandbox}/absent-session-bus
            XDG_CONFIG_HOME=${poison_sandbox}/config
            XDG_DATA_HOME=${poison_sandbox}/data
            XDG_DATA_DIRS=${poison_sandbox}/system-data
            XDG_CACHE_HOME=${poison_sandbox}/cache
            XDG_RUNTIME_DIR=${poison_sandbox}/runtime
            "${SETTINGS_EXECUTABLE}" --page appearance
    WORKING_DIRECTORY "${poison_sandbox}"
    TIMEOUT 3
    RESULT_VARIABLE poison_status
    OUTPUT_VARIABLE poison_output
    ERROR_VARIABLE poison_error
)
# Restore before evaluating the result so even a truthful poison failure does
# not leave the bounded stage incomplete for a diagnostic rerun.
file(RENAME "${withheld_module}" "${appearance_module}")
if(NOT poison_status EQUAL 3)
    message(FATAL_ERROR
        "incomplete installed Settings package returned ${poison_status}, "
        "expected root-construction failure 3 while build QML remained present:\n"
        "${poison_output}${poison_error}")
endif()
# Reinstall rather than trusting the rename restoration, then prove the two
# complete routes below using only the staged prefix.
execute_process(
    COMMAND ${install_command}
    RESULT_VARIABLE reinstall_status
    OUTPUT_VARIABLE reinstall_output
    ERROR_VARIABLE reinstall_error
)
if(NOT reinstall_status EQUAL 0)
    message(FATAL_ERROR
        "staged Settings reinstall failed after package poison:\n"
        "${reinstall_output}${reinstall_error}")
endif()

set(SANDBOX_ROOT "${install_prefix}/route-runtime")
# The installed regression must prove the executable discovers the staged
# theme catalog itself. The build-tree regression still injects its source
# fixture because no install layout exists there.
set(USE_DEFAULT_THEME_SEARCH TRUE)
include("${ROUTE_CHECK}")

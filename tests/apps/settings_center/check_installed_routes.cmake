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

set(network_module
    "${install_prefix}/${INSTALL_QMLDIR}/QindaQt/SettingsApp/Network")
cmake_path(NORMAL_PATH network_module OUTPUT_VARIABLE network_module)
cmake_path(IS_PREFIX install_prefix "${network_module}" NORMALIZE network_in_stage)
if(NOT network_in_stage OR NOT IS_DIRECTORY "${network_module}")
    message(FATAL_ERROR
        "installed Settings Network module is missing or outside stage: "
        "${network_module}")
endif()

set(customize_module
    "${install_prefix}/${INSTALL_QMLDIR}/QindaQt/SettingsApp/Customize")
cmake_path(NORMAL_PATH customize_module OUTPUT_VARIABLE customize_module)
cmake_path(IS_PREFIX install_prefix "${customize_module}" NORMALIZE customize_in_stage)
if(NOT customize_in_stage OR NOT IS_DIRECTORY "${customize_module}")
    message(FATAL_ERROR
        "installed Settings Customize module is missing or outside stage: "
        "${customize_module}")
endif()

foreach(catalog IN ITEMS profiles applets)
    set(catalog_path
        "${install_prefix}/${INSTALL_DATADIR}/qindaqt/${catalog}")
    if(NOT IS_DIRECTORY "${catalog_path}")
        message(FATAL_ERROR
            "installed Customize ${catalog} catalog is missing: ${catalog_path}")
    endif()
endforeach()

set(audio_module
    "${install_prefix}/${INSTALL_QMLDIR}/QindaQt/SettingsApp/Audio")
cmake_path(NORMAL_PATH audio_module OUTPUT_VARIABLE audio_module)
cmake_path(IS_PREFIX install_prefix "${audio_module}" NORMALIZE audio_in_stage)
if(NOT audio_in_stage OR NOT IS_DIRECTORY "${audio_module}")
    message(FATAL_ERROR
        "installed Settings Audio module is missing or outside stage: "
        "${audio_module}")
endif()

set(bluetooth_module
    "${install_prefix}/${INSTALL_QMLDIR}/QindaQt/SettingsApp/Bluetooth")
cmake_path(NORMAL_PATH bluetooth_module OUTPUT_VARIABLE bluetooth_module)
cmake_path(IS_PREFIX install_prefix "${bluetooth_module}" NORMALIZE bluetooth_in_stage)
if(NOT bluetooth_in_stage OR NOT IS_DIRECTORY "${bluetooth_module}")
    message(FATAL_ERROR
        "installed Settings Bluetooth module is missing or outside stage: "
        "${bluetooth_module}")
endif()

set(power_module
    "${install_prefix}/${INSTALL_QMLDIR}/QindaQt/SettingsApp/Power")
cmake_path(NORMAL_PATH power_module OUTPUT_VARIABLE power_module)
cmake_path(IS_PREFIX install_prefix "${power_module}" NORMALIZE power_in_stage)
if(NOT power_in_stage OR NOT IS_DIRECTORY "${power_module}")
    message(FATAL_ERROR
        "installed Settings Power module is missing or outside stage: "
        "${power_module}")
endif()

set(clipboard_module
    "${install_prefix}/${INSTALL_QMLDIR}/QindaQt/SettingsApp/Clipboard")
cmake_path(NORMAL_PATH clipboard_module OUTPUT_VARIABLE clipboard_module)
cmake_path(IS_PREFIX install_prefix "${clipboard_module}" NORMALIZE clipboard_in_stage)
if(NOT clipboard_in_stage OR NOT IS_DIRECTORY "${clipboard_module}")
    message(FATAL_ERROR
        "installed Settings Clipboard module is missing or outside stage: "
        "${clipboard_module}")
endif()

set(color_module
    "${install_prefix}/${INSTALL_QMLDIR}/QindaQt/SettingsApp/Color")
cmake_path(NORMAL_PATH color_module OUTPUT_VARIABLE color_module)
cmake_path(IS_PREFIX install_prefix "${color_module}" NORMALIZE color_in_stage)
if(NOT color_in_stage OR NOT IS_DIRECTORY "${color_module}")
    message(FATAL_ERROR
        "installed Settings Color module is missing or outside stage: "
        "${color_module}")
endif()

set(accessibility_module
    "${install_prefix}/${INSTALL_QMLDIR}/QindaQt/SettingsApp/Accessibility")
cmake_path(NORMAL_PATH accessibility_module OUTPUT_VARIABLE accessibility_module)
cmake_path(IS_PREFIX install_prefix "${accessibility_module}" NORMALIZE accessibility_in_stage)
if(NOT accessibility_in_stage OR NOT IS_DIRECTORY "${accessibility_module}")
    message(FATAL_ERROR
        "installed Settings Accessibility module is missing or outside stage: "
        "${accessibility_module}")
endif()

set(input_module
    "${install_prefix}/${INSTALL_QMLDIR}/QindaQt/SettingsApp/Input")
cmake_path(NORMAL_PATH input_module OUTPUT_VARIABLE input_module)
cmake_path(IS_PREFIX install_prefix "${input_module}" NORMALIZE input_in_stage)
if(NOT input_in_stage OR NOT IS_DIRECTORY "${input_module}")
    message(FATAL_ERROR
        "installed Settings Input module is missing or outside stage: "
        "${input_module}")
endif()

set(build_appearance_module
    "${build_directory}/qml/QindaQt/SettingsApp/Appearance")
if(NOT IS_DIRECTORY "${build_appearance_module}")
    message(FATAL_ERROR
        "package poison requires the developer Appearance QML tree to remain present")
endif()
set(build_network_module "${build_directory}/qml/QindaQt/SettingsApp/Network")
if(NOT IS_DIRECTORY "${build_network_module}")
    message(FATAL_ERROR
        "package poison requires the developer Network QML tree to remain present")
endif()
set(build_audio_module "${build_directory}/qml/QindaQt/SettingsApp/Audio")
if(NOT IS_DIRECTORY "${build_audio_module}")
    message(FATAL_ERROR
        "package poison requires the developer Audio QML tree to remain present")
endif()
set(build_bluetooth_module "${build_directory}/qml/QindaQt/SettingsApp/Bluetooth")
if(NOT IS_DIRECTORY "${build_bluetooth_module}")
    message(FATAL_ERROR
        "package poison requires the developer Bluetooth QML tree to remain present")
endif()
set(build_clipboard_module "${build_directory}/qml/QindaQt/SettingsApp/Clipboard")
if(NOT IS_DIRECTORY "${build_clipboard_module}")
    message(FATAL_ERROR
        "package relocation requires the developer Clipboard QML tree to remain present")
endif()
set(build_accessibility_module "${build_directory}/qml/QindaQt/SettingsApp/Accessibility")
if(NOT IS_DIRECTORY "${build_accessibility_module}")
    message(FATAL_ERROR
        "package poison requires the developer Accessibility QML tree to remain present")
endif()
set(build_input_module "${build_directory}/qml/QindaQt/SettingsApp/Input")
if(NOT IS_DIRECTORY "${build_input_module}")
    message(FATAL_ERROR
        "package relocation requires the developer Input QML tree to remain present")
endif()

set(withheld_module "${appearance_module}.withheld")
set(poison_sandbox "${install_prefix}/package-poison")
file(MAKE_DIRECTORY "${poison_sandbox}/config" "${poison_sandbox}/data"
                    "${poison_sandbox}/system-data" "${poison_sandbox}/cache")
# AGENT-GUARD: The runtime directory must stay short: the session binds a
# Wayland socket inside it and a sockaddr_un sun_path only holds 108 bytes.
# Deep prefixes (this checkout's build path) overflow it and crash the staged
# executable before it can report the poison failure this gate proves, so the
# runtime root lives beside the build tree, not under the deep sandbox.
set(poison_runtime_dir "${CMAKE_BINARY_DIR}/package-poison-runtime")
file(REMOVE_RECURSE "${poison_runtime_dir}")
file(MAKE_DIRECTORY "${poison_runtime_dir}")
file(CHMOD "${poison_runtime_dir}"
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
            XDG_RUNTIME_DIR=${poison_runtime_dir}
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

# Repeat the developer-tree poison for the Network route. A relocated binary
# must never borrow the build module even though it remains present.
set(withheld_network_module "${network_module}.withheld")
file(RENAME "${network_module}" "${withheld_network_module}")
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
            XDG_RUNTIME_DIR=${poison_runtime_dir}
            "${SETTINGS_EXECUTABLE}" --page network
    WORKING_DIRECTORY "${poison_sandbox}"
    TIMEOUT 3
    RESULT_VARIABLE network_poison_status
    OUTPUT_VARIABLE network_poison_output
    ERROR_VARIABLE network_poison_error
)
file(RENAME "${withheld_network_module}" "${network_module}")
if(NOT network_poison_status EQUAL 3)
    message(FATAL_ERROR
        "incomplete installed Settings Network package returned "
        "${network_poison_status}, expected root-construction failure 3 while "
        "build QML remained present:\n"
        "${network_poison_output}${network_poison_error}")
endif()
# Reinstall rather than trusting the rename restoration, then repeat the
# developer-tree poison for the Audio route the same way, and finally prove
# all twelve complete routes below using only the staged prefix.
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

set(withheld_audio_module "${audio_module}.withheld")
file(RENAME "${audio_module}" "${withheld_audio_module}")
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
            XDG_RUNTIME_DIR=${poison_runtime_dir}
            "${SETTINGS_EXECUTABLE}" --page audio
    WORKING_DIRECTORY "${poison_sandbox}"
    TIMEOUT 3
    RESULT_VARIABLE audio_poison_status
    OUTPUT_VARIABLE audio_poison_output
    ERROR_VARIABLE audio_poison_error
)
file(RENAME "${withheld_audio_module}" "${audio_module}")
if(NOT audio_poison_status EQUAL 3)
    message(FATAL_ERROR
        "incomplete installed Settings Audio package returned "
        "${audio_poison_status}, expected root-construction failure 3 while "
        "build QML remained present:\n"
        "${audio_poison_output}${audio_poison_error}")
endif()
execute_process(
    COMMAND ${install_command}
    RESULT_VARIABLE audio_reinstall_status
    OUTPUT_VARIABLE audio_reinstall_output
    ERROR_VARIABLE audio_reinstall_error
)
if(NOT audio_reinstall_status EQUAL 0)
    message(FATAL_ERROR
        "staged Settings reinstall failed after Audio package poison:\n"
        "${audio_reinstall_output}${audio_reinstall_error}")
endif()

# Repeat the developer-tree poison for the Accessibility route: withholding
# its installed module must fail root construction (exit 3) even though the
# build tree still carries the module.
set(withheld_accessibility_module "${accessibility_module}.withheld")
file(RENAME "${accessibility_module}" "${withheld_accessibility_module}")
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
            XDG_RUNTIME_DIR=${poison_runtime_dir}
            "${SETTINGS_EXECUTABLE}" --page accessibility
    WORKING_DIRECTORY "${poison_sandbox}"
    TIMEOUT 3
    RESULT_VARIABLE accessibility_poison_status
    OUTPUT_VARIABLE accessibility_poison_output
    ERROR_VARIABLE accessibility_poison_error
)
file(RENAME "${withheld_accessibility_module}" "${accessibility_module}")
if(NOT accessibility_poison_status EQUAL 3)
    message(FATAL_ERROR
        "incomplete installed Settings Accessibility package returned "
        "${accessibility_poison_status}, expected root-construction failure 3 while "
        "build QML remained present:\n"
        "${accessibility_poison_output}${accessibility_poison_error}")
endif()
execute_process(
    COMMAND ${install_command}
    RESULT_VARIABLE accessibility_reinstall_status
    OUTPUT_VARIABLE accessibility_reinstall_output
    ERROR_VARIABLE accessibility_reinstall_error
)
if(NOT accessibility_reinstall_status EQUAL 0)
    message(FATAL_ERROR
        "staged Settings reinstall failed after Accessibility package poison:\n"
        "${accessibility_reinstall_output}${accessibility_reinstall_error}")
endif()

# Repeat the developer-tree poison for the Input route: withholding its
# installed module must fail root construction (exit 3) even though the
# build tree still carries the module.
set(withheld_input_module "${input_module}.withheld")
file(RENAME "${input_module}" "${withheld_input_module}")
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
            XDG_RUNTIME_DIR=${poison_runtime_dir}
            "${SETTINGS_EXECUTABLE}" --page input
    WORKING_DIRECTORY "${poison_sandbox}"
    TIMEOUT 3
    RESULT_VARIABLE input_poison_status
    OUTPUT_VARIABLE input_poison_output
    ERROR_VARIABLE input_poison_error
)
file(RENAME "${withheld_input_module}" "${input_module}")
if(NOT input_poison_status EQUAL 3)
    message(FATAL_ERROR
        "incomplete installed Settings Input package returned "
        "${input_poison_status}, expected root-construction failure 3 while "
        "build QML remained present:\n"
        "${input_poison_output}${input_poison_error}")
endif()
execute_process(
    COMMAND ${install_command}
    RESULT_VARIABLE input_reinstall_status
    OUTPUT_VARIABLE input_reinstall_output
    ERROR_VARIABLE input_reinstall_error
)
if(NOT input_reinstall_status EQUAL 0)
    message(FATAL_ERROR
        "staged Settings reinstall failed after Input package poison:\n"
        "${input_reinstall_output}${input_reinstall_error}")
endif()

set(SANDBOX_ROOT "${install_prefix}/route-runtime")
# The installed regression must prove the executable discovers the staged
# theme catalog itself. The build-tree regression still injects its source
# fixture because no install layout exists there.
set(USE_DEFAULT_THEME_SEARCH TRUE)
include("${ROUTE_CHECK}")

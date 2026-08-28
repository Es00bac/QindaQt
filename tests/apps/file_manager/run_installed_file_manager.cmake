# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required IN ITEMS
        QINDAQT_CMAKE
        QINDAQT_BUILD_DIRECTORY
        QINDAQT_INSTALL_PREFIX
        QINDAQT_INSTALL_BINDIR
        QINDAQT_INSTALL_DATADIR
        QINDAQT_QML_INSTALL_DIR
        QINDAQT_TOKENS_LIBRARY_NAME
        QINDAQT_TOKENS_PLUGIN_NAME
        QINDAQT_CONTROLS_LIBRARY_NAME
        QINDAQT_CONTROLS_PLUGIN_NAME
        QINDAQT_EXPECTED_CONTROLS_PATHS)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "Missing installed File Manager input: ${required}")
    endif()
endforeach()

cmake_path(NORMAL_PATH QINDAQT_BUILD_DIRECTORY OUTPUT_VARIABLE build_directory)
cmake_path(NORMAL_PATH QINDAQT_INSTALL_PREFIX OUTPUT_VARIABLE install_prefix)
cmake_path(IS_PREFIX build_directory "${install_prefix}" NORMALIZE prefix_is_in_build)
if(NOT prefix_is_in_build OR install_prefix STREQUAL build_directory)
    message(FATAL_ERROR "Refusing to replace a staged prefix outside the test build tree")
endif()

# AGENT-GUARD: A clean, build-confined stage makes removed package rules fail
# instead of passing through an artifact from an earlier test invocation.
file(REMOVE_RECURSE "${install_prefix}")
set(install_command
    "${QINDAQT_CMAKE}" --install "${build_directory}"
    --prefix "${install_prefix}" --component FileManager
)
if(DEFINED QINDAQT_CONFIGURATION AND NOT QINDAQT_CONFIGURATION STREQUAL "")
    list(APPEND install_command --config "${QINDAQT_CONFIGURATION}")
endif()
execute_process(
    COMMAND ${install_command}
    RESULT_VARIABLE install_status
    OUTPUT_VARIABLE install_output
    ERROR_VARIABLE install_error
)
if(NOT install_status STREQUAL "0")
    message(FATAL_ERROR
        "Staged FileManager component install failed:\n${install_output}${install_error}"
    )
endif()

set(file_manager "${install_prefix}/${QINDAQT_INSTALL_BINDIR}/qindaqt-file-manager")
set(desktop
    "${install_prefix}/${QINDAQT_INSTALL_DATADIR}/applications/org.qindaqt.FileManager.desktop"
)
set(theme_root "${install_prefix}/${QINDAQT_INSTALL_DATADIR}/qindaqt/themes")
set(qml_root "${install_prefix}/${QINDAQT_QML_INSTALL_DIR}")
set(tokens_root "${qml_root}/QindaQt/Tokens")
set(controls_root "${qml_root}/QindaQt/Controls")

set(theme_ids
    qinda-dark
    qinda-light
    qinda-dusk
    qinda-macos
    qinda-high-contrast
)
set(required_payload
    "${file_manager}"
    "${desktop}"
    "${tokens_root}/qmldir"
    "${tokens_root}/qindaqt_tokens.qmltypes"
    "${tokens_root}/${QINDAQT_TOKENS_LIBRARY_NAME}"
    "${tokens_root}/${QINDAQT_TOKENS_PLUGIN_NAME}"
    "${controls_root}/qmldir"
    "${controls_root}/qindaqt_controls.qmltypes"
    "${controls_root}/${QINDAQT_CONTROLS_LIBRARY_NAME}"
    "${controls_root}/${QINDAQT_CONTROLS_PLUGIN_NAME}"
)
foreach(theme_id IN LISTS theme_ids)
    list(APPEND required_payload "${theme_root}/${theme_id}.json")
endforeach()
foreach(required IN LISTS required_payload)
    if(NOT EXISTS "${required}")
        message(FATAL_ERROR "Installed File Manager payload is missing: ${required}")
    endif()
endforeach()

file(READ "${desktop}" desktop_contents)
foreach(required_entry IN ITEMS
        "Type=Application"
        "Name=QindaQt File Manager"
        "Exec=qindaqt-file-manager %u"
        "MimeType=inode/directory;"
        "Terminal=false")
    string(FIND "${desktop_contents}" "${required_entry}" entry_position)
    if(entry_position EQUAL -1)
        message(FATAL_ERROR
            "Installed File Manager desktop metadata is missing: ${required_entry}"
        )
    endif()
endforeach()

string(REPLACE "|" ";" expected_controls_paths "${QINDAQT_EXPECTED_CONTROLS_PATHS}")
list(SORT expected_controls_paths)
file(GLOB_RECURSE installed_controls_paths
    RELATIVE "${controls_root}"
    "${controls_root}/*.qml"
)
list(SORT installed_controls_paths)
if(NOT installed_controls_paths STREQUAL expected_controls_paths)
    message(FATAL_ERROR
        "Installed FileManager Controls inventory does not match Qt's module paths:\n"
        "expected=${expected_controls_paths}\ninstalled=${installed_controls_paths}"
    )
endif()

# The test host may still have a valid build tree. Reject any executable that
# embeds that escape hatch before exercising the sanitized staged prefix.
set(build_qml_root "${build_directory}/qml")
file(STRINGS "${file_manager}" executable_strings)
foreach(executable_string IN LISTS executable_strings)
    string(FIND "${executable_string}" "${build_qml_root}" build_path_position)
    if(NOT build_path_position EQUAL -1)
        message(FATAL_ERROR
            "Installed File Manager embeds the build-tree QML path: ${build_qml_root}"
        )
    endif()
endforeach()

set(private_root "${install_prefix}/private-runtime")
set(private_folder "${private_root}/folder")
set(private_runtime "${private_root}/runtime")
file(MAKE_DIRECTORY
    "${private_folder}"
    "${private_runtime}"
    "${private_root}/config"
    "${private_root}/cache"
)
file(CHMOD "${private_runtime}"
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
)
set(probe_environment
    "${QINDAQT_CMAKE}" -E env
    --unset=QML_IMPORT_PATH
    --unset=QML2_IMPORT_PATH
    --unset=LD_LIBRARY_PATH
    --unset=DYLD_LIBRARY_PATH
    QT_QPA_PLATFORM=offscreen
    QT_QUICK_BACKEND=software
    QML_DISABLE_DISK_CACHE=1
    "HOME=${private_root}"
    "XDG_CONFIG_HOME=${private_root}/config"
    "XDG_CACHE_HOME=${private_root}/cache"
    "XDG_DATA_DIRS=${install_prefix}/${QINDAQT_INSTALL_DATADIR}"
    "XDG_RUNTIME_DIR=${private_runtime}"
)

foreach(theme_id IN LISTS theme_ids)
    execute_process(
        COMMAND ${probe_environment}
                "${file_manager}" --theme "${theme_id}" --check-theme
        WORKING_DIRECTORY "${private_root}"
        RESULT_VARIABLE theme_status
        OUTPUT_VARIABLE theme_output
        ERROR_VARIABLE theme_error
    )
    if(NOT theme_status STREQUAL "0")
        message(FATAL_ERROR
            "Installed File Manager ${theme_id} check failed:\n"
            "${theme_output}${theme_error}"
        )
    endif()
    string(STRIP "${theme_output}" theme_output)
    if(NOT theme_output STREQUAL "${theme_id} qst-1")
        message(FATAL_ERROR
            "Installed File Manager ${theme_id} returned '${theme_output}'"
        )
    endif()
endforeach()

execute_process(
    COMMAND ${probe_environment}
            "${file_manager}" --theme qinda-dark --check-qml-root
            "${private_folder}"
    WORKING_DIRECTORY "${private_root}"
    RESULT_VARIABLE root_status
    OUTPUT_VARIABLE root_output
    ERROR_VARIABLE root_error
    TIMEOUT 30
)
if(NOT root_status STREQUAL "0")
    message(FATAL_ERROR
        "Installed File Manager QML root check failed:\n${root_output}${root_error}"
    )
endif()
string(STRIP "${root_output}" root_output)
if(NOT root_output STREQUAL "qml-root-loaded")
    message(FATAL_ERROR
        "Installed File Manager root check returned '${root_output}'"
    )
endif()

message(STATUS
    "Installed FileManager component passed exact payload, build-isolation, "
    "five-theme, and offscreen QML-root checks"
)

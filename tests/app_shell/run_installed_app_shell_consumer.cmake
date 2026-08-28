# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required IN ITEMS QINDAQT_CMAKE QINDAQT_BUILD_DIRECTORY QINDAQT_INSTALL_PREFIX
                          QINDAQT_CONSUMER_SOURCE_DIRECTORY QINDAQT_CONSUMER_SOURCE
                          QINDAQT_APP_SHELL_SOURCE_DIRECTORY
                          QINDAQT_INSTALL_INCLUDEDIR QINDAQT_QML_INSTALL_DIR
                          QINDAQT_APP_SHELL_LIBRARY QINDAQT_APP_SHELL_PLUGIN
                          QINDAQT_TOKENS_LIBRARY QINDAQT_TOKENS_PLUGIN
                          QINDAQT_CONTROLS_LIBRARY QINDAQT_CONTROLS_PLUGIN
                          QINDAQT_QT6_DIR QINDAQT_QMLTESTRUNNER QINDAQT_QML_TEST)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "Missing installed AppShell consumer input: ${required}")
    endif()
endforeach()

cmake_path(NORMAL_PATH QINDAQT_BUILD_DIRECTORY OUTPUT_VARIABLE build_directory)
cmake_path(NORMAL_PATH QINDAQT_INSTALL_PREFIX OUTPUT_VARIABLE install_prefix)
cmake_path(IS_PREFIX build_directory "${install_prefix}" NORMALIZE prefix_is_in_build)
if(NOT prefix_is_in_build OR install_prefix STREQUAL build_directory)
    message(FATAL_ERROR "Refusing to replace an AppShell stage outside the test build tree")
endif()
# AGENT-GUARD: A clean stage proves deleted headers/QML/install rules cannot be
# masked by stale files. The normalized build-prefix guard confines removal.
file(REMOVE_RECURSE "${install_prefix}")

set(qml_stage_root "${install_prefix}/${QINDAQT_QML_INSTALL_DIR}")
set(qml_build_root "${build_directory}/qml")

function(stage_qml_module module_name typeinfo backing_library plugin_library)
    set(build_module "${qml_build_root}/QindaQt/${module_name}")
    set(stage_module "${qml_stage_root}/QindaQt/${module_name}")
    get_filename_component(backing_library_name "${backing_library}" NAME)
    get_filename_component(plugin_library_name "${plugin_library}" NAME)
    foreach(required_file IN ITEMS
            "${build_module}/qmldir"
            "${build_module}/${typeinfo}"
            "${backing_library}"
            "${plugin_library}")
        if(NOT EXISTS "${required_file}")
            message(FATAL_ERROR
                "Focused ${module_name} stage input is missing ${required_file}")
        endif()
    endforeach()
    file(MAKE_DIRECTORY "${stage_module}")
    file(COPY_FILE "${build_module}/qmldir" "${stage_module}/qmldir")
    file(COPY_FILE "${build_module}/${typeinfo}" "${stage_module}/${typeinfo}")
    file(COPY_FILE "${backing_library}"
        "${stage_module}/${backing_library_name}" ONLY_IF_DIFFERENT)
    file(COPY_FILE "${plugin_library}"
        "${stage_module}/${plugin_library_name}" ONLY_IF_DIFFERENT)
    if(EXISTS "${build_module}/qml")
        file(COPY "${build_module}/qml" DESTINATION "${stage_module}")
    endif()
endfunction()

# AGENT-CONTRACT: This proof stages only the three transitive QML modules used
# by AppShell. Running the repository-wide install target would couple a
# focused consumer to unrelated targets that were intentionally not built.
stage_qml_module(
    AppShell qindaqt_app_shell.qmltypes
    "${QINDAQT_APP_SHELL_LIBRARY}" "${QINDAQT_APP_SHELL_PLUGIN}"
)
stage_qml_module(
    Tokens qindaqt_tokens.qmltypes
    "${QINDAQT_TOKENS_LIBRARY}" "${QINDAQT_TOKENS_PLUGIN}"
)
stage_qml_module(
    Controls qindaqt_controls.qmltypes
    "${QINDAQT_CONTROLS_LIBRARY}" "${QINDAQT_CONTROLS_PLUGIN}"
)

file(
    COPY "${QINDAQT_APP_SHELL_SOURCE_DIRECTORY}/include/"
    DESTINATION "${install_prefix}/${QINDAQT_INSTALL_INCLUDEDIR}"
)

set(module_directory "${qml_stage_root}/QindaQt/AppShell")
get_filename_component(app_shell_library_name "${QINDAQT_APP_SHELL_LIBRARY}" NAME)
get_filename_component(app_shell_plugin_name "${QINDAQT_APP_SHELL_PLUGIN}" NAME)
foreach(relative IN ITEMS
        "qml/ApplicationShell.qml"
        "qmldir"
        "qindaqt_app_shell.qmltypes"
        "${app_shell_library_name}"
        "${app_shell_plugin_name}")
    if(NOT EXISTS "${module_directory}/${relative}")
        message(FATAL_ERROR "Focused AppShell stage is missing ${relative}")
    endif()
endforeach()
foreach(relative IN ITEMS
        "qindaqt/app_shell/action_registry.h"
        "qindaqt/app_shell/app_shell_types.h"
        "qindaqt/app_shell/application_coordinator.h")
    if(NOT EXISTS "${install_prefix}/${QINDAQT_INSTALL_INCLUDEDIR}/${relative}")
        message(FATAL_ERROR "Installed AppShell public header is missing ${relative}")
    endif()
endforeach()

set(consumer_build "${install_prefix}/consumer-build")
execute_process(
    COMMAND
        "${QINDAQT_CMAKE}"
        -S "${QINDAQT_CONSUMER_SOURCE_DIRECTORY}"
        -B "${consumer_build}"
        -G "${QINDAQT_GENERATOR}"
        "-DCMAKE_BUILD_TYPE=${QINDAQT_BUILD_TYPE}"
        "-DQt6_DIR=${QINDAQT_QT6_DIR}"
        "-DQINDAQT_STAGE_INCLUDE_DIR=${install_prefix}/${QINDAQT_INSTALL_INCLUDEDIR}"
        "-DQINDAQT_APP_SHELL_LIBRARY=${module_directory}/${app_shell_library_name}"
        "-DQINDAQT_APP_SHELL_LIBRARY_DIR=${module_directory}"
        "-DQINDAQT_QML_STAGE_ROOT=${qml_stage_root}"
        "-DQINDAQT_CONSUMER_SOURCE=${QINDAQT_CONSUMER_SOURCE}"
    RESULT_VARIABLE configure_status
    OUTPUT_VARIABLE configure_output
    ERROR_VARIABLE configure_error
)
if(NOT configure_status EQUAL 0)
    message(FATAL_ERROR "Installed AppShell consumer configure failed:\n${configure_output}${configure_error}")
endif()
execute_process(
    COMMAND "${QINDAQT_CMAKE}" --build "${consumer_build}" --parallel 1
    RESULT_VARIABLE build_status
    OUTPUT_VARIABLE build_output
    ERROR_VARIABLE build_error
)
if(NOT build_status EQUAL 0)
    message(FATAL_ERROR "Installed AppShell consumer build failed:\n${build_output}${build_error}")
endif()
execute_process(
    COMMAND
        "${QINDAQT_CMAKE}" -E env
        "QT_QPA_PLATFORM=offscreen"
        "LD_LIBRARY_PATH=${qml_stage_root}/QindaQt/AppShell:${qml_stage_root}/QindaQt/Controls:${qml_stage_root}/QindaQt/Tokens"
        "${consumer_build}/qindaqt_installed_app_shell_consumer"
    RESULT_VARIABLE consumer_status
    OUTPUT_VARIABLE consumer_output
    ERROR_VARIABLE consumer_error
)
if(NOT consumer_status EQUAL 0)
    message(FATAL_ERROR
        "Installed AppShell C++ consumer exited ${consumer_status}:\n"
        "${consumer_output}${consumer_error}")
endif()

get_filename_component(qml_import_root "${module_directory}/../.." ABSOLUTE)
execute_process(
    COMMAND
        "${QINDAQT_CMAKE}" -E env
        "QT_QPA_PLATFORM=offscreen"
        "QT_QUICK_BACKEND=software"
        "QML2_IMPORT_PATH="
        "QML_IMPORT_PATH="
        "LD_LIBRARY_PATH=${qml_stage_root}/QindaQt/AppShell:${qml_stage_root}/QindaQt/Controls:${qml_stage_root}/QindaQt/Tokens"
        "${QINDAQT_QMLTESTRUNNER}"
        -import "${qml_import_root}"
        -input "${QINDAQT_QML_TEST}"
    RESULT_VARIABLE qml_status
    OUTPUT_VARIABLE qml_output
    ERROR_VARIABLE qml_error
)
if(NOT qml_status EQUAL 0)
    message(FATAL_ERROR
        "Installed AppShell QML consumer exited ${qml_status}:\n"
        "${qml_output}${qml_error}")
endif()

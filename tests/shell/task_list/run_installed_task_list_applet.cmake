# SPDX-License-Identifier: GPL-3.0-or-later

# AGENT-GUARD: this probe validates the real TaskListAppletRuntime install
# rules by running `cmake --install --component TaskListAppletRuntime` into a
# fresh stage inside the test build tree and then building/running a consumer
# against only staged files. A component that installs nothing succeeds
# silently, so every required artifact is asserted present afterwards.
# Sibling artifacts the applet depends on but does not own (T0/T1 task-list
# archives/headers, the themes/design-tokens archives and theme file, and the
# Controls/Tokens QML modules) are copied from the build/source tree into the
# stage exactly like the clipboard/AppShell installed-consumer precedent:
# their install components belong to their owners. After staging, no consumer
# input references a build- or source-tree path.

foreach(required IN ITEMS QINDAQT_CMAKE QINDAQT_BUILD_DIRECTORY QINDAQT_SOURCE_DIRECTORY
                          QINDAQT_INSTALL_PREFIX
                          QINDAQT_INSTALL_BINDIR QINDAQT_INSTALL_INCLUDEDIR
                          QINDAQT_INSTALL_LIBDIR QINDAQT_INSTALL_DATADIR
                          QINDAQT_QML_INSTALL_DIR QINDAQT_CONFIGURATION
                          QINDAQT_TASK_LIST_LIBRARY QINDAQT_TASK_LIST_PRODUCER_LIBRARY
                          QINDAQT_TASK_LIST_OPERATIONS_LIBRARY
                          QINDAQT_TASK_LIST_INCLUDE_DIR QINDAQT_TASK_LIST_PRODUCER_INCLUDE_DIR
                          QINDAQT_TASK_LIST_OPERATIONS_INCLUDE_DIR
                          QINDAQT_CONTROLS_MODULE_DIRECTORY QINDAQT_TOKENS_MODULE_DIRECTORY
                          QINDAQT_CONTROLS_LIBRARY QINDAQT_TOKENS_LIBRARY
                          QINDAQT_THEMES_LIBRARY QINDAQT_DESIGN_TOKENS_LIBRARY
                          QINDAQT_THEMES_INCLUDE_DIR QINDAQT_DESIGN_TOKENS_INCLUDE_DIR
                          QINDAQT_THEME_FILE
                          QINDAQT_CONSUMER_SOURCE_DIRECTORY QINDAQT_CONSUMER_SOURCE
                          QINDAQT_QT6_DIR QINDAQT_GENERATOR QINDAQT_BUILD_TYPE
                          QINDAQT_STATIC_LIBRARY_SUFFIX QINDAQT_SHARED_LIBRARY_SUFFIX)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "Missing installed task-list applet consumer input: ${required}")
    endif()
endforeach()

cmake_path(NORMAL_PATH QINDAQT_BUILD_DIRECTORY OUTPUT_VARIABLE build_directory)
cmake_path(NORMAL_PATH QINDAQT_INSTALL_PREFIX OUTPUT_VARIABLE install_prefix)
cmake_path(IS_PREFIX build_directory "${install_prefix}" NORMALIZE prefix_is_in_build)
if(NOT prefix_is_in_build OR install_prefix STREQUAL build_directory)
    message(FATAL_ERROR "Refusing to replace a task-list applet stage outside the test build tree")
endif()
set(relocated_prefix "${install_prefix}-relocated")
cmake_path(IS_PREFIX build_directory "${relocated_prefix}" NORMALIZE relocated_is_in_build)
if(NOT relocated_is_in_build)
    message(FATAL_ERROR "Refusing to relocate a task-list applet stage outside the test build tree")
endif()
file(REMOVE_RECURSE "${install_prefix}")
file(REMOVE_RECURSE "${relocated_prefix}")

set(install_command "${QINDAQT_CMAKE}" --install "${build_directory}"
                    --prefix "${install_prefix}"
                    --component TaskListAppletRuntime)
if(NOT QINDAQT_CONFIGURATION STREQUAL "")
    list(APPEND install_command --config "${QINDAQT_CONFIGURATION}")
endif()
execute_process(
    COMMAND ${install_command}
    RESULT_VARIABLE install_status
    OUTPUT_VARIABLE install_output
    ERROR_VARIABLE install_error
)
if(NOT install_status EQUAL 0)
    message(FATAL_ERROR
            "TaskListAppletRuntime component install failed:\n${install_output}${install_error}")
endif()

set(qml_stage_root "${install_prefix}/${QINDAQT_QML_INSTALL_DIR}")
set(module_directory "${qml_stage_root}/QindaQt/Shell/TaskList")
set(stage_include "${install_prefix}/${QINDAQT_INSTALL_INCLUDEDIR}")
set(stage_lib "${install_prefix}/${QINDAQT_INSTALL_LIBDIR}")

# The component must have packaged its own public boundary and runtime.
foreach(relative IN ITEMS
        "qmldir"
        "qindaqt_shell_task_list_applet.qmltypes"
        "qml/TaskListApplet.qml"
        "qml/TaskListEntryButton.qml"
        "libqindaqt_shell_task_list_applet${QINDAQT_STATIC_LIBRARY_SUFFIX}"
        "libqindaqt_shell_task_list_appletplugin${QINDAQT_STATIC_LIBRARY_SUFFIX}")
    if(NOT EXISTS "${module_directory}/${relative}")
        message(FATAL_ERROR
                "TaskListAppletRuntime component stage is missing ${relative} — the "
                "install rule did not actually package the QML module")
    endif()
endforeach()
foreach(relative IN ITEMS
        "qindaqt/shell/task_list/applet/task_list_applet_types.h"
        "qindaqt/shell/task_list/applet/task_list_applet_projection.h"
        "qindaqt/shell/task_list/applet/task_list_applet_operation_port.h"
        "qindaqt/shell/task_list/applet/task_list_applet_operation_bridge.h"
        "qindaqt/shell/task_list/applet/task_list_applet_controller.h")
    if(NOT EXISTS "${stage_include}/${relative}")
        message(FATAL_ERROR
                "TaskListAppletRuntime component stage is missing public header ${relative}")
    endif()
endforeach()

set(manifest_path "${install_prefix}/${QINDAQT_INSTALL_DATADIR}/qindaqt/applets/task-list.json")
if(NOT EXISTS "${manifest_path}")
    message(FATAL_ERROR "TaskListAppletRuntime component stage is missing the applet manifest")
endif()
file(READ "${manifest_path}" manifest_content)
if(NOT manifest_content MATCHES "\"id\"[ \t]*:[ \t]*\"task-list\"")
    message(FATAL_ERROR "TaskListAppletRuntime component staged a manifest that is not the task-list applet")
endif()

# The component also carries the live shell binary (narrow-package precedent).
if(NOT EXISTS "${install_prefix}/${QINDAQT_INSTALL_BINDIR}/qindaqt-shell")
    message(FATAL_ERROR "TaskListAppletRuntime component stage is missing the qindaqt-shell binary")
endif()

# Stage the sibling task-list archives and public headers whose install
# components belong to the T0/T1 owners.
foreach(archive_input IN ITEMS
        "${QINDAQT_TASK_LIST_LIBRARY}" "${QINDAQT_TASK_LIST_PRODUCER_LIBRARY}"
        "${QINDAQT_TASK_LIST_OPERATIONS_LIBRARY}")
    if(NOT EXISTS "${archive_input}")
        message(FATAL_ERROR "Focused task-list applet stage input is missing ${archive_input}")
    endif()
    get_filename_component(archive_name "${archive_input}" NAME)
    file(COPY_FILE "${archive_input}" "${stage_lib}/${archive_name}" ONLY_IF_DIFFERENT)
endforeach()
foreach(include_input IN ITEMS
        "${QINDAQT_TASK_LIST_INCLUDE_DIR}/qindaqt"
        "${QINDAQT_TASK_LIST_PRODUCER_INCLUDE_DIR}/qindaqt"
        "${QINDAQT_TASK_LIST_OPERATIONS_INCLUDE_DIR}/qindaqt")
    if(NOT IS_DIRECTORY "${include_input}")
        message(FATAL_ERROR "Focused task-list applet stage input is missing ${include_input}")
    endif()
    file(COPY "${include_input}" DESTINATION "${stage_include}")
endforeach()

# Stage the transitive sibling QML modules Controls and Tokens from the build
# tree; their install components belong to other owners (clipboard/AppShell
# precedent). The applet QML imports both at runtime.
foreach(module_directory_input IN ITEMS
        "${QINDAQT_CONTROLS_MODULE_DIRECTORY}" "${QINDAQT_TOKENS_MODULE_DIRECTORY}")
    if(NOT EXISTS "${module_directory_input}/qmldir")
        message(FATAL_ERROR "Focused task-list applet stage input is missing ${module_directory_input}/qmldir")
    endif()
    file(COPY "${module_directory_input}"
         DESTINATION "${qml_stage_root}/QindaQt")
endforeach()
foreach(stage_module IN ITEMS Controls Tokens)
    if(stage_module STREQUAL "Controls")
        set(backing_library "${QINDAQT_CONTROLS_LIBRARY}")
    else()
        set(backing_library "${QINDAQT_TOKENS_LIBRARY}")
    endif()
    if(NOT EXISTS "${backing_library}")
        message(FATAL_ERROR "Focused task-list applet stage input is missing ${backing_library}")
    endif()
    get_filename_component(backing_name "${backing_library}" NAME)
    file(COPY_FILE "${backing_library}"
         "${qml_stage_root}/QindaQt/${stage_module}/${backing_name}"
         ONLY_IF_DIFFERENT)
endforeach()

# Stage the themes/design-tokens archives and headers and the theme file:
# public dependencies whose packaging belongs to their own module owners. The
# consumer publishes the staged theme through the staged Tokens module.
foreach(archive_input IN ITEMS
        "${QINDAQT_THEMES_LIBRARY}" "${QINDAQT_DESIGN_TOKENS_LIBRARY}")
    if(NOT EXISTS "${archive_input}")
        message(FATAL_ERROR "Focused task-list applet stage input is missing ${archive_input}")
    endif()
    get_filename_component(archive_name "${archive_input}" NAME)
    file(COPY_FILE "${archive_input}" "${stage_lib}/${archive_name}" ONLY_IF_DIFFERENT)
endforeach()
foreach(include_input IN ITEMS
        "${QINDAQT_THEMES_INCLUDE_DIR}/qindaqt"
        "${QINDAQT_DESIGN_TOKENS_INCLUDE_DIR}/qindaqt")
    if(NOT IS_DIRECTORY "${include_input}")
        message(FATAL_ERROR "Focused task-list applet stage input is missing ${include_input}")
    endif()
    file(COPY "${include_input}" DESTINATION "${stage_include}")
endforeach()
if(NOT EXISTS "${QINDAQT_THEME_FILE}")
    message(FATAL_ERROR "Focused task-list applet stage input is missing ${QINDAQT_THEME_FILE}")
endif()
file(MAKE_DIRECTORY "${install_prefix}/data/themes")
file(COPY_FILE "${QINDAQT_THEME_FILE}"
     "${install_prefix}/data/themes/qinda-light.json" ONLY_IF_DIFFERENT)

set(consumer_build "${install_prefix}/consumer-build")
execute_process(
    COMMAND
        "${QINDAQT_CMAKE}"
        -S "${QINDAQT_CONSUMER_SOURCE_DIRECTORY}"
        -B "${consumer_build}"
        -G "${QINDAQT_GENERATOR}"
        "-DCMAKE_BUILD_TYPE=${QINDAQT_BUILD_TYPE}"
        "-DQt6_DIR=${QINDAQT_QT6_DIR}"
        "-DQINDAQT_STAGE_ROOT=${install_prefix}"
        "-DQINDAQT_STAGE_INCLUDE_DIR=${stage_include}"
        "-DQINDAQT_STAGE_LIBDIR=${QINDAQT_INSTALL_LIBDIR}"
        "-DQINDAQT_STAGE_QML_ROOT=${qml_stage_root}"
        "-DQINDAQT_CONSUMER_SOURCE=${QINDAQT_CONSUMER_SOURCE}"
        "-DQINDAQT_STATIC_LIBRARY_SUFFIX=${QINDAQT_STATIC_LIBRARY_SUFFIX}"
    RESULT_VARIABLE configure_status
    OUTPUT_VARIABLE configure_output
    ERROR_VARIABLE configure_error
)
if(NOT configure_status EQUAL 0)
    message(FATAL_ERROR
            "Installed task-list applet consumer configure failed:\n${configure_output}${configure_error}")
endif()
execute_process(
    COMMAND "${QINDAQT_CMAKE}" --build "${consumer_build}" --parallel 1
    RESULT_VARIABLE build_status
    OUTPUT_VARIABLE build_output
    ERROR_VARIABLE build_error
)
if(NOT build_status EQUAL 0)
    message(FATAL_ERROR
            "Installed task-list applet consumer build failed:\n${build_output}${build_error}")
endif()

set(consumer_executable "${consumer_build}/qindaqt_installed_task_list_applet_consumer")

# The copied build-tree shared libraries carry build-tree RUNPATHs; a staged
# package must be self-contained and relocatable. Rewrite each staged copy to
# an $ORIGIN-relative RUNPATH so a moved stage resolves its own siblings —
# Controls needs the staged Tokens library next to it (clipboard precedent).
find_program(readelf_program readelf REQUIRED)
find_program(patchelf_program patchelf REQUIRED)
foreach(stage_module IN ITEMS Controls Tokens)
    set(staged_module_directory "${qml_stage_root}/QindaQt/${stage_module}")
    file(GLOB staged_module_shared_objects
         "${staged_module_directory}/*${QINDAQT_SHARED_LIBRARY_SUFFIX}")
    if(NOT staged_module_shared_objects)
        message(FATAL_ERROR
                "Staged ${stage_module} module has no shared artifacts for relocation proof")
    endif()
    if(stage_module STREQUAL "Controls")
        set(module_rpath "$ORIGIN:$ORIGIN/../Tokens")
    else()
        set(module_rpath "$ORIGIN")
    endif()
    foreach(patch_target IN LISTS staged_module_shared_objects)
        execute_process(
            COMMAND "${patchelf_program}" --set-rpath "${module_rpath}" "${patch_target}"
            RESULT_VARIABLE patchelf_status
            OUTPUT_VARIABLE patchelf_output
            ERROR_VARIABLE patchelf_error
        )
        if(NOT patchelf_status EQUAL 0)
            message(FATAL_ERROR
                    "patchelf failed on ${patch_target}:\n${patchelf_output}${patchelf_error}")
        endif()
    endforeach()
endforeach()

# CMake auto-adds the directories of full-path-linked shared libraries to the
# build RPATH; a relocatable package must not carry them. Enforce the exact
# $ORIGIN-relative RUNPATH on the staged consumer — the assertions below then
# verify the final artifact, not the build intent.
cmake_path(RELATIVE_PATH qml_stage_root
           BASE_DIRECTORY "${install_prefix}"
           OUTPUT_VARIABLE qml_root_relative_for_rpath)
execute_process(
    COMMAND "${patchelf_program}" --set-rpath
            "$ORIGIN/../${qml_root_relative_for_rpath}/QindaQt/Controls:$ORIGIN/../${qml_root_relative_for_rpath}/QindaQt/Tokens"
            "${consumer_executable}"
    RESULT_VARIABLE consumer_patchelf_status
    OUTPUT_VARIABLE consumer_patchelf_output
    ERROR_VARIABLE consumer_patchelf_error
)
if(NOT consumer_patchelf_status EQUAL 0)
    message(FATAL_ERROR
            "patchelf failed on the installed task-list applet consumer:\n"
            "${consumer_patchelf_output}${consumer_patchelf_error}")
endif()

# RPATH truth: every staged dynamic artifact — the Controls/Tokens backing
# libraries, their QML plugins, and the consumer — resolves through
# $ORIGIN-relative entries only; a directly linked consumer could otherwise
# mask a plugin's absolute build-tree RUNPATH.
execute_process(
    COMMAND "${readelf_program}" -d "${consumer_executable}"
    RESULT_VARIABLE readelf_status
    OUTPUT_VARIABLE readelf_output
    ERROR_QUIET
)
if(NOT readelf_status EQUAL 0)
    message(FATAL_ERROR "readelf failed on the installed task-list applet consumer")
endif()
if(NOT readelf_output MATCHES "[$]ORIGIN/[.][.]/.*QindaQt/Controls")
    message(FATAL_ERROR
            "Installed consumer RPATH is not $ORIGIN-relative for the staged Controls module:\n${readelf_output}")
endif()
if(NOT readelf_output MATCHES "[$]ORIGIN/[.][.]/.*QindaQt/Tokens")
    message(FATAL_ERROR
            "Installed consumer RPATH is not $ORIGIN-relative for the staged Tokens module:\n${readelf_output}")
endif()
file(GLOB_RECURSE staged_dynamic_artifacts
     "${qml_stage_root}/QindaQt/*${QINDAQT_SHARED_LIBRARY_SUFFIX}")
list(APPEND staged_dynamic_artifacts "${consumer_executable}")
foreach(dynamic_artifact IN LISTS staged_dynamic_artifacts)
    execute_process(
        COMMAND "${readelf_program}" -d "${dynamic_artifact}"
        RESULT_VARIABLE artifact_readelf_status
        OUTPUT_VARIABLE artifact_readelf_output
        ERROR_VARIABLE artifact_readelf_error
    )
    if(NOT artifact_readelf_status EQUAL 0)
        message(FATAL_ERROR
                "readelf failed on staged dynamic artifact ${dynamic_artifact}:\n"
                "${artifact_readelf_error}")
    endif()
    if(NOT artifact_readelf_output MATCHES "[$]ORIGIN")
        message(FATAL_ERROR
                "Staged dynamic artifact lacks an $ORIGIN-relative RPATH: "
                "${dynamic_artifact}\n${artifact_readelf_output}")
    endif()
    if(artifact_readelf_output MATCHES "${install_prefix}"
       OR artifact_readelf_output MATCHES "${build_directory}"
       OR artifact_readelf_output MATCHES "${QINDAQT_SOURCE_DIRECTORY}")
        message(FATAL_ERROR
                "Staged dynamic artifact leaks an absolute stage/build/source-tree path: "
                "${dynamic_artifact}\n${artifact_readelf_output}")
    endif()
endforeach()

# AGENT-GUARD: the consumer must run from staged files only — no
# LD_LIBRARY_PATH, no ambient import paths — first at the original prefix...
execute_process(
    COMMAND
        "${QINDAQT_CMAKE}" -E env
        "--unset=LD_LIBRARY_PATH"
        "QT_QPA_PLATFORM=offscreen"
        "QT_QUICK_BACKEND=software"
        "QT_FATAL_WARNINGS=1"
        "QML2_IMPORT_PATH="
        "QML_IMPORT_PATH="
        "${consumer_executable}"
    RESULT_VARIABLE consumer_status
    OUTPUT_VARIABLE consumer_output
    ERROR_VARIABLE consumer_error
)
if(NOT consumer_status EQUAL 0)
    message(FATAL_ERROR
            "Installed task-list applet consumer exited ${consumer_status}:\n"
            "${consumer_output}${consumer_error}")
endif()

# ...and then from a RELOCATED prefix: the whole stage moves, so any absolute
# RPATH or compile-time stage path breaks this run.
get_filename_component(consumer_executable_name "${consumer_executable}" NAME)
file(RENAME "${install_prefix}" "${relocated_prefix}")
execute_process(
    COMMAND
        "${QINDAQT_CMAKE}" -E env
        "--unset=LD_LIBRARY_PATH"
        "QT_QPA_PLATFORM=offscreen"
        "QT_QUICK_BACKEND=software"
        "QT_FATAL_WARNINGS=1"
        "QML2_IMPORT_PATH="
        "QML_IMPORT_PATH="
        "${relocated_prefix}/consumer-build/${consumer_executable_name}"
    RESULT_VARIABLE relocated_status
    OUTPUT_VARIABLE relocated_output
    ERROR_VARIABLE relocated_error
)
file(RENAME "${relocated_prefix}" "${install_prefix}")
if(NOT relocated_status EQUAL 0)
    message(FATAL_ERROR
            "Relocated task-list applet consumer exited ${relocated_status} "
            "(stage moved to ${relocated_prefix}):\n"
            "${relocated_output}${relocated_error}")
endif()

# The production shell must resolve only explicitly selected installed data,
# even when every ambient/source-style path points at hostile input.
set(shell "${install_prefix}/${QINDAQT_INSTALL_BINDIR}/qindaqt-shell")
set(installed_data "${install_prefix}/${QINDAQT_INSTALL_DATADIR}/qindaqt")
foreach(required_path IN ITEMS
        "${installed_data}/profiles/qindaqt.json"
        "${installed_data}/themes/qinda-dark.json"
        "${installed_data}/applet-policy/default.json")
    if(NOT EXISTS "${required_path}")
        message(FATAL_ERROR
                "TaskListAppletRuntime component stage is missing ${required_path}")
    endif()
endforeach()
file(STRINGS "${shell}" compiled_boundary
     REGEX "(QindaQt\\.Shell\\.TaskList|TaskListApplet)")
if(NOT compiled_boundary)
    message(FATAL_ERROR
            "Staged shell contains no compiled Task List composition evidence")
endif()

set(poison "${install_prefix}/source-poison")
file(MAKE_DIRECTORY "${poison}/profiles" "${poison}/themes"
                    "${poison}/applets" "${poison}/runtime")
file(WRITE "${poison}/applets/broken.json" "{ not-json")
file(WRITE "${poison}/profiles/broken.json" "{ not-json")
file(WRITE "${poison}/themes/broken.json" "{ not-json")
file(WRITE "${poison}/policy.json" "{ not-json")
execute_process(
    COMMAND "${QINDAQT_CMAKE}" -E env
            --unset=LD_LIBRARY_PATH --unset=DYLD_LIBRARY_PATH
            --unset=DBUS_SESSION_BUS_ADDRESS --unset=DISPLAY
            --unset=WAYLAND_DISPLAY
            QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software
            "XDG_RUNTIME_DIR=${poison}/runtime"
            "XDG_DATA_DIRS=${poison}" "XDG_DATA_HOME=${poison}"
            "QINDAQT_PROFILE_DIR=${poison}/profiles"
            "QINDAQT_THEME_DIR=${poison}/themes"
            "QINDAQT_APPLET_DIR=${poison}/applets"
            "QINDAQT_APPLET_POLICY=${poison}/policy.json"
            "${shell}" --list
            "--profile-dir=${installed_data}/profiles"
            "--theme-dir=${installed_data}/themes"
            "--applet-dir=${installed_data}/applets"
            "--applet-policy=${installed_data}/applet-policy/default.json"
    RESULT_VARIABLE list_status
    OUTPUT_VARIABLE list_output
    ERROR_VARIABLE list_error)
if(NOT list_status EQUAL 0)
    message(FATAL_ERROR
            "Staged Task List shell failed under source poison:\n"
            "${list_output}${list_error}")
endif()
string(FIND "${list_output}" "task-list - Task List" task_list_entry)
if(task_list_entry EQUAL -1)
    message(FATAL_ERROR
            "Staged shell did not resolve Task List:\n${list_output}")
endif()

message(STATUS
        "Installed task-list applet package, relocation, RPATH, boundary, and source-poison probes passed")

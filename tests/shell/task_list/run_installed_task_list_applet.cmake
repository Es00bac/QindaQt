# SPDX-License-Identifier: GPL-3.0-or-later

# AGENT-GUARD: this probe validates the real TaskListAppletRuntime install
# rules by running `cmake --install --component TaskListAppletRuntime` into a
# fresh stage inside the test build tree and then building/running a consumer
# against only staged files. A component that installs nothing succeeds
# silently, so every required artifact is asserted present afterwards.
# Sibling task-list archives/headers (T0 core, T1 producer/operations) are
# copied from the build/source tree into the stage exactly like the
# clipboard/AppShell installed-consumer precedent: their install components
# belong to the T0/T1 owners. After staging, no consumer input references a
# build- or source-tree path.

foreach(required IN ITEMS QINDAQT_CMAKE QINDAQT_BUILD_DIRECTORY QINDAQT_SOURCE_DIRECTORY
                          QINDAQT_INSTALL_PREFIX
                          QINDAQT_INSTALL_BINDIR QINDAQT_INSTALL_INCLUDEDIR
                          QINDAQT_INSTALL_LIBDIR QINDAQT_INSTALL_DATADIR
                          QINDAQT_QML_INSTALL_DIR QINDAQT_CONFIGURATION
                          QINDAQT_TASK_LIST_LIBRARY QINDAQT_TASK_LIST_PRODUCER_LIBRARY
                          QINDAQT_TASK_LIST_OPERATIONS_LIBRARY
                          QINDAQT_TASK_LIST_INCLUDE_DIR QINDAQT_TASK_LIST_PRODUCER_INCLUDE_DIR
                          QINDAQT_TASK_LIST_OPERATIONS_INCLUDE_DIR
                          QINDAQT_CONSUMER_SOURCE_DIRECTORY QINDAQT_CONSUMER_SOURCE
                          QINDAQT_QT6_DIR QINDAQT_GENERATOR QINDAQT_BUILD_TYPE
                          QINDAQT_STATIC_LIBRARY_SUFFIX)
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

# Relocation truth: the consumer links only staged static archives plus system
# Qt, so its dynamic section must carry no absolute stage, build, or source
# path. If CMake still baked an absolute stage RPATH (it must not), rewrite it
# to an $ORIGIN-relative one; a build/source leak is a hard failure.
find_program(readelf_program readelf REQUIRED)
find_program(patchelf_program patchelf REQUIRED)
execute_process(
    COMMAND "${readelf_program}" -d "${consumer_executable}"
    RESULT_VARIABLE readelf_status
    OUTPUT_VARIABLE readelf_output
    ERROR_QUIET
)
if(NOT readelf_status EQUAL 0)
    message(FATAL_ERROR "readelf failed on the installed task-list applet consumer")
endif()
if(readelf_output MATCHES "${install_prefix}")
    execute_process(
        COMMAND "${patchelf_program}" --set-rpath "$ORIGIN" "${consumer_executable}"
        RESULT_VARIABLE patchelf_status
        OUTPUT_VARIABLE patchelf_output
        ERROR_VARIABLE patchelf_error
    )
    if(NOT patchelf_status EQUAL 0)
        message(FATAL_ERROR
                "patchelf failed on the installed task-list applet consumer:\n"
                "${patchelf_output}${patchelf_error}")
    endif()
    execute_process(
        COMMAND "${readelf_program}" -d "${consumer_executable}"
        RESULT_VARIABLE readelf_status
        OUTPUT_VARIABLE readelf_output
        ERROR_QUIET
    )
    if(NOT readelf_status EQUAL 0)
        message(FATAL_ERROR "readelf failed on the patched task-list applet consumer")
    endif()
endif()
if(readelf_output MATCHES "${install_prefix}"
   OR readelf_output MATCHES "${build_directory}"
   OR readelf_output MATCHES "${QINDAQT_SOURCE_DIRECTORY}")
    message(FATAL_ERROR
            "Installed task-list applet consumer leaks an absolute stage/build/source path:\n"
            "${readelf_output}")
endif()

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
message(STATUS "Installed task-list applet package, relocation, RPATH, and boundary probe passed")

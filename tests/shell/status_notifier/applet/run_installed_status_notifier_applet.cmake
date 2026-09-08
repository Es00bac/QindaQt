# SPDX-License-Identifier: GPL-3.0-or-later

# AGENT-GUARD: this probe validates the real StatusNotifierAppletRuntime
# install rules by running `cmake --install --component StatusNotifierAppletRuntime`
# into a fresh stage and then building/running a consumer against only staged
# files. A component that installs nothing succeeds silently, so every
# required artifact is asserted present afterwards. Sibling artifacts the
# applet depends on but does not own (Controls/Tokens modules, the S1
# status-notifier archives and headers, the themes and design-tokens archives,
# the theme file) are copied from the build/source tree into the stage exactly
# like the AppShell/Clipboard installed-consumer precedent: their own install
# components belong to other owners. After staging, no consumer input
# references a build- or source-tree path.

foreach(required IN ITEMS QINDAQT_CMAKE QINDAQT_BUILD_DIRECTORY QINDAQT_SOURCE_DIRECTORY
                          QINDAQT_INSTALL_PREFIX
                          QINDAQT_INSTALL_INCLUDEDIR QINDAQT_INSTALL_LIBDIR
                          QINDAQT_QML_INSTALL_DIR
                          QINDAQT_INSTALL_DATADIR QINDAQT_CONFIGURATION
                          QINDAQT_CONTROLS_MODULE_DIRECTORY QINDAQT_TOKENS_MODULE_DIRECTORY
                          QINDAQT_CONTROLS_LIBRARY QINDAQT_TOKENS_LIBRARY
                          QINDAQT_STATUS_NOTIFIER_LIBRARY
                          QINDAQT_DBUSMENU_LIBRARY QINDAQT_MENU_PROTOCOL_LIBRARY
                          QINDAQT_STATUS_NOTIFIER_ITEM_CLIENT_LIBRARY
                          QINDAQT_STATUS_NOTIFIER_ICON_LIBRARY
                          QINDAQT_THEMES_LIBRARY QINDAQT_DESIGN_TOKENS_LIBRARY
                          QINDAQT_STATUS_NOTIFIER_INCLUDE_DIR
                          QINDAQT_STATUS_NOTIFIER_ITEM_CLIENT_INCLUDE_DIR
                          QINDAQT_STATUS_NOTIFIER_ICON_INCLUDE_DIR
                          QINDAQT_THEMES_INCLUDE_DIR QINDAQT_DESIGN_TOKENS_INCLUDE_DIR
                          QINDAQT_THEME_FILE
                          QINDAQT_CONSUMER_SOURCE_DIRECTORY QINDAQT_CONSUMER_SOURCE
                          QINDAQT_QT6_DIR QINDAQT_GENERATOR QINDAQT_BUILD_TYPE
                          QINDAQT_STATIC_LIBRARY_SUFFIX QINDAQT_SHARED_LIBRARY_SUFFIX)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "Missing installed Status Notifier applet consumer input: ${required}")
    endif()
endforeach()

cmake_path(NORMAL_PATH QINDAQT_BUILD_DIRECTORY OUTPUT_VARIABLE build_directory)
cmake_path(NORMAL_PATH QINDAQT_INSTALL_PREFIX OUTPUT_VARIABLE install_prefix)
cmake_path(IS_PREFIX build_directory "${install_prefix}" NORMALIZE prefix_is_in_build)
if(NOT prefix_is_in_build OR install_prefix STREQUAL build_directory)
    message(FATAL_ERROR "Refusing to replace a Status Notifier applet stage outside the test build tree")
endif()
set(relocated_prefix "${install_prefix}-relocated")
cmake_path(IS_PREFIX build_directory "${relocated_prefix}" NORMALIZE relocated_is_in_build)
if(NOT relocated_is_in_build)
    message(FATAL_ERROR "Refusing to relocate a Status Notifier applet stage outside the test build tree")
endif()
file(REMOVE_RECURSE "${install_prefix}")
file(REMOVE_RECURSE "${relocated_prefix}")

set(install_command "${QINDAQT_CMAKE}" --install "${build_directory}"
                    --prefix "${install_prefix}"
                    --component StatusNotifierAppletRuntime)
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
            "StatusNotifierAppletRuntime component install failed:\n${install_output}${install_error}")
endif()

set(qml_stage_root "${install_prefix}/${QINDAQT_QML_INSTALL_DIR}")
set(module_directory "${qml_stage_root}/QindaQt/Shell/StatusNotifier")
set(stage_include "${install_prefix}/${QINDAQT_INSTALL_INCLUDEDIR}")
set(stage_lib "${install_prefix}/${QINDAQT_INSTALL_LIBDIR}")

# The component must have packaged its own public boundary and runtime.
foreach(relative IN ITEMS
        "qmldir"
        "qindaqt_shell_status_notifier_applet.qmltypes"
        "qml/StatusNotifierApplet.qml"
        "qml/StatusNotifierItemDelegate.qml")
    if(NOT EXISTS "${module_directory}/${relative}")
        message(FATAL_ERROR
                "StatusNotifierAppletRuntime component stage is missing ${relative} — the install rule "
                "did not actually package the QML module")
    endif()
endforeach()
foreach(relative IN ITEMS
        "qindaqt/shell/status_notifier/applet/status_notifier_applet_types.h"
        "qindaqt/shell/status_notifier/applet/status_notifier_applet_model.h"
        "qindaqt/shell/status_notifier/applet/status_notifier_source_interface.h"
        "qindaqt/shell/status_notifier/applet/status_notifier_applet_controller.h"
        "qindaqt/shell/status_notifier/applet/status_notifier_monitor_adapter.h")
    if(NOT EXISTS "${stage_include}/${relative}")
        message(FATAL_ERROR
                "StatusNotifierAppletRuntime component stage is missing public header ${relative}")
    endif()
endforeach()

foreach(expected_archive IN ITEMS
        "${module_directory}/libqindaqt_shell_status_notifier_applet_runtime${QINDAQT_STATIC_LIBRARY_SUFFIX}"
        "${module_directory}/libqindaqt_shell_status_notifier_applet_runtimeplugin${QINDAQT_STATIC_LIBRARY_SUFFIX}"
        "${stage_lib}/libqindaqt_shell_status_notifier_applet${QINDAQT_STATIC_LIBRARY_SUFFIX}")
    if(NOT EXISTS "${expected_archive}")
        message(FATAL_ERROR
                "StatusNotifierAppletRuntime component stage is missing archive ${expected_archive}")
    endif()
endforeach()

set(manifest_path "${install_prefix}/${QINDAQT_INSTALL_DATADIR}/qindaqt/applets/status-notifier.json")
if(NOT EXISTS "${manifest_path}")
    message(FATAL_ERROR "StatusNotifierAppletRuntime component stage is missing the applet manifest")
endif()
file(READ "${manifest_path}" manifest_content)
if(NOT manifest_content MATCHES "\"id\"[ \t]*:[ \t]*\"status-notifier\"")
    message(FATAL_ERROR "StatusNotifierAppletRuntime component staged a manifest that is not the status-notifier applet")
endif()

# Stage the transitive sibling QML modules Controls and Tokens from the build
# tree; their install components belong to other owners (AppShell precedent).
foreach(module_directory_input IN ITEMS
        "${QINDAQT_CONTROLS_MODULE_DIRECTORY}" "${QINDAQT_TOKENS_MODULE_DIRECTORY}")
    if(NOT EXISTS "${module_directory_input}/qmldir")
        message(FATAL_ERROR "Focused Status Notifier applet stage input is missing ${module_directory_input}/qmldir")
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
        message(FATAL_ERROR "Focused Status Notifier applet stage input is missing ${backing_library}")
    endif()
    get_filename_component(backing_name "${backing_library}" NAME)
    file(COPY_FILE "${backing_library}"
         "${qml_stage_root}/QindaQt/${stage_module}/${backing_name}"
         ONLY_IF_DIFFERENT)
endforeach()

# The copied build-tree shared libraries carry build-tree RUNPATHs; a staged
# package must be self-contained and relocatable. Rewrite each staged copy to
# an $ORIGIN-relative RUNPATH so a moved stage resolves its own siblings —
# Controls needs the staged Tokens library next to it.
find_program(patchelf_program patchelf)
if(NOT patchelf_program)
    message(FATAL_ERROR "patchelf is required for the StatusNotifierAppletRuntime relocation check")
endif()
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

# Stage the S1 status-notifier archives/headers, the themes/design-tokens
# archives and headers, and the theme file: public dependencies whose
# packaging belongs to their own module owners.
foreach(archive_input IN ITEMS
        "${QINDAQT_STATUS_NOTIFIER_LIBRARY}" "${QINDAQT_STATUS_NOTIFIER_ITEM_CLIENT_LIBRARY}"
        "${QINDAQT_STATUS_NOTIFIER_ICON_LIBRARY}" "${QINDAQT_THEMES_LIBRARY}"
        "${QINDAQT_DESIGN_TOKENS_LIBRARY}"
        "${QINDAQT_DBUSMENU_LIBRARY}" "${QINDAQT_MENU_PROTOCOL_LIBRARY}")
    if(NOT EXISTS "${archive_input}")
        message(FATAL_ERROR "Focused Status Notifier applet stage input is missing ${archive_input}")
    endif()
    get_filename_component(archive_name "${archive_input}" NAME)
    file(COPY_FILE "${archive_input}" "${stage_lib}/${archive_name}" ONLY_IF_DIFFERENT)
endforeach()
foreach(include_input IN ITEMS
        "${QINDAQT_STATUS_NOTIFIER_INCLUDE_DIR}/qindaqt"
        "${QINDAQT_STATUS_NOTIFIER_ITEM_CLIENT_INCLUDE_DIR}/qindaqt"
        "${QINDAQT_STATUS_NOTIFIER_ICON_INCLUDE_DIR}/qindaqt"
        "${QINDAQT_THEMES_INCLUDE_DIR}/qindaqt"
        "${QINDAQT_DESIGN_TOKENS_INCLUDE_DIR}/qindaqt")
    if(NOT EXISTS "${include_input}")
        message(FATAL_ERROR "Focused Status Notifier applet stage input is missing ${include_input}")
    endif()
    file(COPY "${include_input}" DESTINATION "${stage_include}")
endforeach()
if(NOT EXISTS "${QINDAQT_THEME_FILE}")
    message(FATAL_ERROR "Focused Status Notifier applet stage input is missing ${QINDAQT_THEME_FILE}")
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
    RESULT_VARIABLE configure_status
    OUTPUT_VARIABLE configure_output
    ERROR_VARIABLE configure_error
)
if(NOT configure_status EQUAL 0)
    message(FATAL_ERROR
            "Installed Status Notifier applet consumer configure failed:\n${configure_output}${configure_error}")
endif()
execute_process(
    COMMAND "${QINDAQT_CMAKE}" --build "${consumer_build}" --parallel 1
    RESULT_VARIABLE build_status
    OUTPUT_VARIABLE build_output
    ERROR_VARIABLE build_error
)
if(NOT build_status EQUAL 0)
    message(FATAL_ERROR
            "Installed Status Notifier applet consumer build failed:\n${build_output}${build_error}")
endif()

# CMake auto-adds the directories of full-path-linked shared libraries to the
# build RPATH; a relocatable package must not carry them. Enforce the exact
# $ORIGIN-relative RUNPATH on the staged consumer — the readelf assertions
# below then verify the final artifact, not the build intent.
cmake_path(RELATIVE_PATH qml_stage_root
           BASE_DIRECTORY "${install_prefix}"
           OUTPUT_VARIABLE qml_root_relative_for_rpath)
execute_process(
    COMMAND "${patchelf_program}" --set-rpath
            "$ORIGIN/../${qml_root_relative_for_rpath}/QindaQt/Controls:$ORIGIN/../${qml_root_relative_for_rpath}/QindaQt/Tokens"
            "${consumer_build}/qindaqt_installed_status_notifier_applet_consumer"
    RESULT_VARIABLE consumer_patchelf_status
    OUTPUT_VARIABLE consumer_patchelf_output
    ERROR_VARIABLE consumer_patchelf_error
)
if(NOT consumer_patchelf_status EQUAL 0)
    message(FATAL_ERROR
            "patchelf failed on the installed Status Notifier applet consumer:\n"
            "${consumer_patchelf_output}${consumer_patchelf_error}")
endif()

# RPATH truth: every staged dynamic artifact, including the optional Controls
# and Tokens QML plugins, resolves through $ORIGIN-relative entries only. The
# consumer can preload the backing libraries and otherwise mask a plugin's
# absolute build-tree RUNPATH, so checking only the executable is insufficient.
find_program(readelf_program readelf)
if(NOT readelf_program)
    message(FATAL_ERROR "readelf is required for the StatusNotifierAppletRuntime RPATH check")
endif()
execute_process(
    COMMAND "${readelf_program}" -d "${consumer_build}/qindaqt_installed_status_notifier_applet_consumer"
    RESULT_VARIABLE readelf_status
    OUTPUT_VARIABLE readelf_output
    ERROR_QUIET
)
if(NOT readelf_status EQUAL 0)
    message(FATAL_ERROR "readelf failed on the installed Status Notifier applet consumer")
endif()
if(NOT readelf_output MATCHES "[$]ORIGIN/[.][.]/.*QindaQt/Controls")
    message(FATAL_ERROR
            "Installed consumer RPATH is not $ORIGIN-relative for the staged Controls module:\n${readelf_output}")
endif()
if(NOT readelf_output MATCHES "[$]ORIGIN/[.][.]/.*QindaQt/Tokens")
    message(FATAL_ERROR
            "Installed consumer RPATH is not $ORIGIN-relative for the staged Tokens module:\n${readelf_output}")
endif()

# AGENT-NOTE (mirrors clipboard P2-2): e3e2dba rewrote the backing libraries
# and consumer but left the copied QML plugin libraries pointing at the build
# tree. Enumerate every staged ELF shared object so a directly linked consumer
# cannot hide a contaminated plugin RUNPATH.
file(GLOB_RECURSE staged_dynamic_artifacts
     "${qml_stage_root}/QindaQt/*${QINDAQT_SHARED_LIBRARY_SUFFIX}")
list(APPEND staged_dynamic_artifacts
     "${consumer_build}/qindaqt_installed_status_notifier_applet_consumer")
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
        "QML2_IMPORT_PATH="
        "QML_IMPORT_PATH="
        "${consumer_build}/qindaqt_installed_status_notifier_applet_consumer"
    RESULT_VARIABLE consumer_status
    OUTPUT_VARIABLE consumer_output
    ERROR_VARIABLE consumer_error
)
if(NOT consumer_status EQUAL 0)
    message(FATAL_ERROR
            "Installed Status Notifier applet consumer exited ${consumer_status}:\n"
            "${consumer_output}${consumer_error}")
endif()

# ...and then from a RELOCATED prefix: the whole stage moves, so any absolute
# RPATH or compile-time stage path breaks this run.
get_filename_component(consumer_executable_name
                       "${consumer_build}/qindaqt_installed_status_notifier_applet_consumer" NAME)
file(RENAME "${install_prefix}" "${relocated_prefix}")
execute_process(
    COMMAND
        "${QINDAQT_CMAKE}" -E env
        "--unset=LD_LIBRARY_PATH"
        "QT_QPA_PLATFORM=offscreen"
        "QT_QUICK_BACKEND=software"
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
            "Relocated Status Notifier applet consumer exited ${relocated_status} "
            "(stage moved to ${relocated_prefix}):\n"
            "${relocated_output}${relocated_error}")
endif()
message(STATUS "Installed Status Notifier applet package, relocation, RPATH, and boundary probe passed")

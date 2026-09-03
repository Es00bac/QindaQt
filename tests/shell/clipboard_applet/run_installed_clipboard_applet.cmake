# SPDX-License-Identifier: GPL-3.0-or-later

# AGENT-GUARD: this probe validates the real ClipboardApplet install rules by
# running `cmake --install --component ClipboardApplet` into a fresh stage and
# then building/running a consumer against only staged files. A component that
# installs nothing succeeds silently, so every required artifact is asserted
# present afterwards. Sibling artifacts the applet depends on but does not own
# (Controls/Tokens modules, the C0 model archive and headers, the themes and
# design-tokens archives, the theme file) are copied from the build/source
# tree into the stage exactly like the AppShell installed-consumer precedent:
# their own install components belong to other owners. After staging, no
# consumer input references a build- or source-tree path.

foreach(required IN ITEMS QINDAQT_CMAKE QINDAQT_BUILD_DIRECTORY QINDAQT_SOURCE_DIRECTORY
                          QINDAQT_INSTALL_PREFIX
                          QINDAQT_INSTALL_INCLUDEDIR QINDAQT_INSTALL_LIBDIR
                          QINDAQT_QML_INSTALL_DIR
                          QINDAQT_INSTALL_DATADIR QINDAQT_CONFIGURATION
                          QINDAQT_CONTROLS_MODULE_DIRECTORY QINDAQT_TOKENS_MODULE_DIRECTORY
                          QINDAQT_CONTROLS_LIBRARY QINDAQT_TOKENS_LIBRARY
                          QINDAQT_CLIPBOARD_MODEL_LIBRARY QINDAQT_THEMES_LIBRARY
                          QINDAQT_DESIGN_TOKENS_LIBRARY
                          QINDAQT_CLIPBOARD_MODEL_INCLUDE_DIR QINDAQT_THEMES_INCLUDE_DIR
                          QINDAQT_DESIGN_TOKENS_INCLUDE_DIR QINDAQT_THEME_FILE
                          QINDAQT_CONSUMER_SOURCE_DIRECTORY QINDAQT_CONSUMER_SOURCE
                          QINDAQT_QT6_DIR QINDAQT_GENERATOR QINDAQT_BUILD_TYPE
                          QINDAQT_STATIC_LIBRARY_SUFFIX QINDAQT_SHARED_LIBRARY_SUFFIX)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "Missing installed Clipboard applet consumer input: ${required}")
    endif()
endforeach()

cmake_path(NORMAL_PATH QINDAQT_BUILD_DIRECTORY OUTPUT_VARIABLE build_directory)
cmake_path(NORMAL_PATH QINDAQT_INSTALL_PREFIX OUTPUT_VARIABLE install_prefix)
cmake_path(IS_PREFIX build_directory "${install_prefix}" NORMALIZE prefix_is_in_build)
if(NOT prefix_is_in_build OR install_prefix STREQUAL build_directory)
    message(FATAL_ERROR "Refusing to replace a Clipboard applet stage outside the test build tree")
endif()
set(relocated_prefix "${install_prefix}-relocated")
cmake_path(IS_PREFIX build_directory "${relocated_prefix}" NORMALIZE relocated_is_in_build)
if(NOT relocated_is_in_build)
    message(FATAL_ERROR "Refusing to relocate a Clipboard applet stage outside the test build tree")
endif()
file(REMOVE_RECURSE "${install_prefix}")
file(REMOVE_RECURSE "${relocated_prefix}")

set(install_command "${QINDAQT_CMAKE}" --install "${build_directory}"
                    --prefix "${install_prefix}"
                    --component ClipboardApplet)
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
            "ClipboardApplet component install failed:\n${install_output}${install_error}")
endif()

set(qml_stage_root "${install_prefix}/${QINDAQT_QML_INSTALL_DIR}")
set(module_directory "${qml_stage_root}/QindaQt/Shell/ClipboardApplet")
set(stage_include "${install_prefix}/${QINDAQT_INSTALL_INCLUDEDIR}")
set(stage_lib "${install_prefix}/${QINDAQT_INSTALL_LIBDIR}")

# The component must have packaged its own public boundary and runtime.
foreach(relative IN ITEMS
        "qmldir"
        "qindaqt_shell_clipboard_applet.qmltypes"
        "qml/ClipboardApplet.qml"
        "qml/ClipboardEntryRow.qml")
    if(NOT EXISTS "${module_directory}/${relative}")
        message(FATAL_ERROR
                "ClipboardApplet component stage is missing ${relative} — the install rule "
                "did not actually package the QML module")
    endif()
endforeach()
foreach(relative IN ITEMS
        "qindaqt/shell/clipboard_applet/clipboard_applet_types.h"
        "qindaqt/shell/clipboard_applet/clipboard_client_interface.h"
        "qindaqt/shell/clipboard_applet/clipboard_applet_model.h"
        "qindaqt/shell/clipboard_applet/clipboard_applet_controller.h"
        "qindaqt/shell/clipboard_applet/clipboard_model_client_adapter.h")
    if(NOT EXISTS "${stage_include}/${relative}")
        message(FATAL_ERROR
                "ClipboardApplet component stage is missing public header ${relative}")
    endif()
endforeach()

foreach(expected_archive IN ITEMS
        "${module_directory}/libqindaqt_shell_clipboard_applet_runtime${QINDAQT_STATIC_LIBRARY_SUFFIX}"
        "${module_directory}/libqindaqt_shell_clipboard_applet_runtimeplugin${QINDAQT_STATIC_LIBRARY_SUFFIX}"
        "${stage_lib}/libqindaqt_shell_clipboard_applet${QINDAQT_STATIC_LIBRARY_SUFFIX}")
    if(NOT EXISTS "${expected_archive}")
        message(FATAL_ERROR
                "ClipboardApplet component stage is missing archive ${expected_archive}")
    endif()
endforeach()

set(manifest_path "${install_prefix}/${QINDAQT_INSTALL_DATADIR}/qindaqt/applets/clipboard.json")
if(NOT EXISTS "${manifest_path}")
    message(FATAL_ERROR "ClipboardApplet component stage is missing the applet manifest")
endif()
file(READ "${manifest_path}" manifest_content)
if(NOT manifest_content MATCHES "\"id\"[ \t]*:[ \t]*\"clipboard\"")
    message(FATAL_ERROR "ClipboardApplet component staged a manifest that is not the clipboard applet")
endif()

# Stage the transitive sibling QML modules Controls and Tokens from the build
# tree; their install components belong to other owners (AppShell precedent).
foreach(module_directory_input IN ITEMS
        "${QINDAQT_CONTROLS_MODULE_DIRECTORY}" "${QINDAQT_TOKENS_MODULE_DIRECTORY}")
    if(NOT EXISTS "${module_directory_input}/qmldir")
        message(FATAL_ERROR "Focused Clipboard applet stage input is missing ${module_directory_input}/qmldir")
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
        message(FATAL_ERROR "Focused Clipboard applet stage input is missing ${backing_library}")
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
    message(FATAL_ERROR "patchelf is required for the ClipboardApplet relocation check")
endif()
set(staged_controls_library "${qml_stage_root}/QindaQt/Controls")
set(staged_tokens_library "${qml_stage_root}/QindaQt/Tokens")
file(GLOB staged_controls_so "${staged_controls_library}/libqindaqt_controls_qml${QINDAQT_SHARED_LIBRARY_SUFFIX}")
file(GLOB staged_tokens_so "${staged_tokens_library}/libqindaqt_tokens_qml${QINDAQT_SHARED_LIBRARY_SUFFIX}")
foreach(patch_spec IN ITEMS
        "${staged_controls_so}|$ORIGIN/../Tokens"
        "${staged_tokens_so}|$ORIGIN")
    string(FIND "${patch_spec}" "|" separator)
    string(SUBSTRING "${patch_spec}" 0 ${separator} patch_target)
    math(EXPR rest_start "${separator} + 1")
    string(SUBSTRING "${patch_spec}" ${rest_start} -1 patch_rpath)
    if(NOT EXISTS "${patch_target}")
        message(FATAL_ERROR "Staged module library missing for RPATH rewrite: ${patch_target}")
    endif()
    execute_process(
        COMMAND "${patchelf_program}" --set-rpath "${patch_rpath}" "${patch_target}"
        RESULT_VARIABLE patchelf_status
        OUTPUT_VARIABLE patchelf_output
        ERROR_VARIABLE patchelf_error
    )
    if(NOT patchelf_status EQUAL 0)
        message(FATAL_ERROR
                "patchelf failed on ${patch_target}:\n${patchelf_output}${patchelf_error}")
    endif()
endforeach()

# Stage the C0 model archive/headers, the themes/design-tokens archives and
# headers, and the theme file: public dependencies whose packaging belongs to
# their own module owners.
foreach(archive_input IN ITEMS
        "${QINDAQT_CLIPBOARD_MODEL_LIBRARY}" "${QINDAQT_THEMES_LIBRARY}"
        "${QINDAQT_DESIGN_TOKENS_LIBRARY}")
    if(NOT EXISTS "${archive_input}")
        message(FATAL_ERROR "Focused Clipboard applet stage input is missing ${archive_input}")
    endif()
    get_filename_component(archive_name "${archive_input}" NAME)
    file(COPY_FILE "${archive_input}" "${stage_lib}/${archive_name}" ONLY_IF_DIFFERENT)
endforeach()
foreach(include_input IN ITEMS
        "${QINDAQT_CLIPBOARD_MODEL_INCLUDE_DIR}/qindaqt"
        "${QINDAQT_THEMES_INCLUDE_DIR}/qindaqt"
        "${QINDAQT_DESIGN_TOKENS_INCLUDE_DIR}/qindaqt")
    if(NOT EXISTS "${include_input}")
        message(FATAL_ERROR "Focused Clipboard applet stage input is missing ${include_input}")
    endif()
    file(COPY "${include_input}" DESTINATION "${stage_include}")
endforeach()
if(NOT EXISTS "${QINDAQT_THEME_FILE}")
    message(FATAL_ERROR "Focused Clipboard applet stage input is missing ${QINDAQT_THEME_FILE}")
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
            "Installed Clipboard applet consumer configure failed:\n${configure_output}${configure_error}")
endif()
execute_process(
    COMMAND "${QINDAQT_CMAKE}" --build "${consumer_build}" --parallel 1
    RESULT_VARIABLE build_status
    OUTPUT_VARIABLE build_output
    ERROR_VARIABLE build_error
)
if(NOT build_status EQUAL 0)
    message(FATAL_ERROR
            "Installed Clipboard applet consumer build failed:\n${build_output}${build_error}")
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
            "${consumer_build}/qindaqt_installed_clipboard_applet_consumer"
    RESULT_VARIABLE consumer_patchelf_status
    OUTPUT_VARIABLE consumer_patchelf_output
    ERROR_VARIABLE consumer_patchelf_error
)
if(NOT consumer_patchelf_status EQUAL 0)
    message(FATAL_ERROR
            "patchelf failed on the installed Clipboard applet consumer:\n"
            "${consumer_patchelf_output}${consumer_patchelf_error}")
endif()

# RPATH truth: the consumer resolves its staged shared dependencies
# (Controls/Tokens) through $ORIGIN-relative entries only — never an absolute
# stage path, so the whole stage can be relocated.
find_program(readelf_program readelf)
if(NOT readelf_program)
    message(FATAL_ERROR "readelf is required for the ClipboardApplet RPATH check")
endif()
execute_process(
    COMMAND "${readelf_program}" -d "${consumer_build}/qindaqt_installed_clipboard_applet_consumer"
    RESULT_VARIABLE readelf_status
    OUTPUT_VARIABLE readelf_output
    ERROR_QUIET
)
if(NOT readelf_status EQUAL 0)
    message(FATAL_ERROR "readelf failed on the installed Clipboard applet consumer")
endif()
if(NOT readelf_output MATCHES "[$]ORIGIN/[.][.]/.*QindaQt/Controls")
    message(FATAL_ERROR
            "Installed consumer RPATH is not $ORIGIN-relative for the staged Controls module:\n${readelf_output}")
endif()
if(NOT readelf_output MATCHES "[$]ORIGIN/[.][.]/.*QindaQt/Tokens")
    message(FATAL_ERROR
            "Installed consumer RPATH is not $ORIGIN-relative for the staged Tokens module:\n${readelf_output}")
endif()
# Poison: no RPATH entry may be an absolute path into the stage, the build
# tree, or the source tree.
if(readelf_output MATCHES "${install_prefix}"
   OR readelf_output MATCHES "${build_directory}"
   OR readelf_output MATCHES "${QINDAQT_SOURCE_DIRECTORY}")
    message(FATAL_ERROR
            "Installed consumer RPATH leaks an absolute stage/build/source-tree path:\n${readelf_output}")
endif()

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
        "${consumer_build}/qindaqt_installed_clipboard_applet_consumer"
    RESULT_VARIABLE consumer_status
    OUTPUT_VARIABLE consumer_output
    ERROR_VARIABLE consumer_error
)
if(NOT consumer_status EQUAL 0)
    message(FATAL_ERROR
            "Installed Clipboard applet consumer exited ${consumer_status}:\n"
            "${consumer_output}${consumer_error}")
endif()

# ...and then from a RELOCATED prefix: the whole stage moves, so any absolute
# RPATH or compile-time stage path breaks this run.
get_filename_component(consumer_executable_name
                       "${consumer_build}/qindaqt_installed_clipboard_applet_consumer" NAME)
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
            "Relocated Clipboard applet consumer exited ${relocated_status} "
            "(stage moved to ${relocated_prefix}):\n"
            "${relocated_output}${relocated_error}")
endif()
message(STATUS "Installed Clipboard applet package, relocation, RPATH, and boundary probe passed")

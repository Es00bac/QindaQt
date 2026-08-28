# SPDX-License-Identifier: GPL-3.0-or-later

# AGENT-GUARD: this probe validates the real ClipboardApplet install rules by
# running `cmake --install --component ClipboardApplet` into a fresh stage and
# then linking/running a consumer against only staged files. A component that
# installs nothing succeeds silently, so every required artifact is asserted
# present afterwards; Controls/Tokens are staged from the build tree exactly
# like the AppShell installed-consumer precedent because their own components
# belong to other owners.

foreach(required IN ITEMS QINDAQT_CMAKE QINDAQT_BUILD_DIRECTORY QINDAQT_INSTALL_PREFIX
                          QINDAQT_INSTALL_INCLUDEDIR QINDAQT_QML_INSTALL_DIR
                          QINDAQT_INSTALL_DATADIR QINDAQT_CONFIGURATION
                          QINDAQT_CONTROLS_MODULE_DIRECTORY QINDAQT_TOKENS_MODULE_DIRECTORY
                          QINDAQT_CONTROLS_LIBRARY QINDAQT_TOKENS_LIBRARY
                          QINDAQT_CONSUMER_SOURCE_DIRECTORY QINDAQT_CONSUMER_SOURCE
                          QINDAQT_QMLTESTRUNNER QINDAQT_QML_TEST
                          QINDAQT_QT6_DIR QINDAQT_GENERATOR QINDAQT_BUILD_TYPE
                          QINDAQT_SHARED_LIBRARY_SUFFIX)
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
file(REMOVE_RECURSE "${install_prefix}")

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

# The component must have packaged its own public boundary and runtime.
foreach(relative IN ITEMS
        "qmldir"
        "qindaqt_shell_clipboard_applet.qmltypes"
        "qml/ClipboardApplet.qml"
        "qml/ClipboardEntryRow.qml"
        "qindaqt/shell/clipboard_applet/clipboard_applet_types.h"
        "qindaqt/shell/clipboard_applet/clipboard_client_interface.h"
        "qindaqt/shell/clipboard_applet/clipboard_applet_model.h"
        "qindaqt/shell/clipboard_applet/clipboard_applet_controller.h"
        "qindaqt/shell/clipboard_applet/clipboard_model_client_adapter.h"
        "qindaqt/services/clipboard_model/clipboard_types.h")
    if(NOT EXISTS "${module_directory}/${relative}"
       AND NOT EXISTS "${install_prefix}/${QINDAQT_INSTALL_INCLUDEDIR}/${relative}")
        message(FATAL_ERROR
                "ClipboardApplet component stage is missing ${relative} — the install rule "
                "did not actually package the public boundary")
    endif()
endforeach()

file(GLOB packaged_module_libraries
     LIST_DIRECTORIES FALSE
     "${module_directory}/libqindaqt_shell_clipboard_applet${QINDAQT_SHARED_LIBRARY_SUFFIX}*"
     "${module_directory}/*qindaqt_shell_clipboard_appletplugin${QINDAQT_SHARED_LIBRARY_SUFFIX}*")
list(LENGTH packaged_module_libraries packaged_module_library_count)
if(packaged_module_library_count LESS 2)
    message(FATAL_ERROR
            "ClipboardApplet component stage is missing the backing library or QML plugin")
endif()

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
# The backing libraries build outside the QML output directories, so they are
# staged beside their plugins exactly like run_installed_app_shell_consumer.
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
        "-DQINDAQT_CLIPBOARD_APPLET_LIBRARY=${module_directory}/libqindaqt_shell_clipboard_applet${QINDAQT_SHARED_LIBRARY_SUFFIX}"
        "-DQINDAQT_QML_STAGE_ROOT=${qml_stage_root}"
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
execute_process(
    COMMAND
        "${QINDAQT_CMAKE}" -E env
        "QT_QPA_PLATFORM=offscreen"
        "LD_LIBRARY_PATH=${module_directory}:${qml_stage_root}/QindaQt/Controls:${qml_stage_root}/QindaQt/Tokens"
        "${consumer_build}/qindaqt_installed_clipboard_applet_consumer"
    RESULT_VARIABLE consumer_status
    OUTPUT_VARIABLE consumer_output
    ERROR_VARIABLE consumer_error
)
if(NOT consumer_status EQUAL 0)
    message(FATAL_ERROR
            "Installed Clipboard applet C++ consumer exited ${consumer_status}:\n"
            "${consumer_output}${consumer_error}")
endif()

# The module lives three levels below the QML root (QindaQt/Shell/ClipboardApplet).
get_filename_component(qml_import_root "${module_directory}/../../.." ABSOLUTE)
execute_process(
    COMMAND
        "${QINDAQT_CMAKE}" -E env
        "QT_QPA_PLATFORM=offscreen"
        "QT_QUICK_BACKEND=software"
        "QML2_IMPORT_PATH="
        "QML_IMPORT_PATH="
        "LD_LIBRARY_PATH=${module_directory}:${qml_stage_root}/QindaQt/Controls:${qml_stage_root}/QindaQt/Tokens"
        "${QINDAQT_QMLTESTRUNNER}"
        -import "${qml_import_root}"
        -input "${QINDAQT_QML_TEST}"
    RESULT_VARIABLE qml_status
    OUTPUT_VARIABLE qml_output
    ERROR_VARIABLE qml_error
)
if(NOT qml_status EQUAL 0)
    message(FATAL_ERROR
            "Installed Clipboard applet QML consumer exited ${qml_status}:\n"
            "${qml_output}${qml_error}")
endif()

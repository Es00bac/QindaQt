# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required IN ITEMS QINDAQT_CMAKE QINDAQT_BUILD_ROOT QINDAQT_STAGE
                          QINDAQT_PROBE QINDAQT_INSTALL_QMLDIR
                          QINDAQT_INSTALL_DATADIR QINDAQT_BUILD_QMLDIR)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "Missing installed launcher input: ${required}")
    endif()
endforeach()

cmake_path(NORMAL_PATH QINDAQT_BUILD_ROOT OUTPUT_VARIABLE build_root)
cmake_path(NORMAL_PATH QINDAQT_STAGE OUTPUT_VARIABLE stage)
cmake_path(IS_PREFIX build_root "${stage}" NORMALIZE stage_is_in_build)
if(NOT stage_is_in_build OR stage STREQUAL build_root)
    message(FATAL_ERROR "Launcher stage must be a child of its build tree")
endif()
file(REMOVE_RECURSE "${stage}")

execute_process(
    COMMAND "${QINDAQT_CMAKE}" --install "${build_root}"
            --prefix "${stage}" --component LauncherAppletRuntime
    RESULT_VARIABLE install_status
    OUTPUT_VARIABLE install_output
    ERROR_VARIABLE install_error)
if(NOT install_status EQUAL 0)
    message(FATAL_ERROR
        "Launcher stage install failed:\n${install_output}${install_error}")
endif()

set(staged_qml "${stage}/${QINDAQT_INSTALL_QMLDIR}")
set(staged_data "${stage}/${QINDAQT_INSTALL_DATADIR}/qindaqt")
foreach(required_path IN ITEMS
        "${staged_qml}/QindaQt/Shell/Launcher/qmldir"
        "${staged_qml}/QindaQt/Shell/Launcher/LauncherApplet.qml"
        "${staged_data}/applets/launcher.json"
        "${staged_data}/applet-policy/default.json")
    if(NOT EXISTS "${required_path}")
        message(FATAL_ERROR "Launcher stage is missing ${required_path}")
    endif()
endforeach()
# The compiled plugin must be staged; a source-only module is not the
# production artifact.
file(GLOB staged_plugins
    "${staged_qml}/QindaQt/Shell/Launcher/*qindaqt_shell_launcher*plugin*")
if(NOT staged_plugins)
    message(FATAL_ERROR "Launcher stage contains no compiled QML plugin")
endif()

# The probe runs with poisoned ambient roots so only the staged paths can
# satisfy it.
set(poison "${stage}/source-poison")
file(MAKE_DIRECTORY "${poison}/applets" "${poison}/runtime")
file(WRITE "${poison}/applets/broken.json" "{ not-json")

execute_process(
    COMMAND "${QINDAQT_CMAKE}" -E env
            QT_QPA_PLATFORM=offscreen
            QT_QUICK_BACKEND=software
            QML_IMPORT_TRACE=1
            "XDG_RUNTIME_DIR=${poison}/runtime"
            "XDG_DATA_DIRS=${poison}"
            "XDG_DATA_HOME=${poison}"
            "${QINDAQT_PROBE}"
            "--applets-dir=${staged_data}/applets"
            "--policy=${staged_data}/applet-policy/default.json"
            "--staged-qml=${staged_qml}"
            "--fallback-qml=${QINDAQT_BUILD_QMLDIR}"
    RESULT_VARIABLE probe_status
    OUTPUT_VARIABLE probe_output
    ERROR_VARIABLE probe_error)
if(NOT probe_status EQUAL 0)
    message(FATAL_ERROR
        "Installed launcher probe failed:\n${probe_output}${probe_error}")
endif()
# Provenance: the compiled module's plugin library must have loaded from the
# staged prefix, not from the build tree.
string(FIND "${probe_output}${probe_error}" "${staged_qml}" staged_hit)
if(staged_hit EQUAL -1)
    message(FATAL_ERROR
        "Probe never loaded from the staged prefix:\n${probe_output}${probe_error}")
endif()

message(STATUS "Installed compiled launcher applet and source-poison proof passed")

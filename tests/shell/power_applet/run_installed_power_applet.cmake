# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required IN ITEMS QINDAQT_CMAKE QINDAQT_BUILD_ROOT QINDAQT_STAGE
                          QINDAQT_INSTALL_BINDIR QINDAQT_INSTALL_DATADIR)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "Missing installed Power applet input: ${required}")
    endif()
endforeach()

cmake_path(NORMAL_PATH QINDAQT_BUILD_ROOT OUTPUT_VARIABLE build_root)
cmake_path(NORMAL_PATH QINDAQT_STAGE OUTPUT_VARIABLE stage)
cmake_path(IS_PREFIX build_root "${stage}" NORMALIZE stage_is_in_build)
if(NOT stage_is_in_build OR stage STREQUAL build_root)
    message(FATAL_ERROR "Power applet stage must be a child of its build tree")
endif()
file(REMOVE_RECURSE "${stage}")

execute_process(
    COMMAND "${QINDAQT_CMAKE}" --install "${build_root}"
            --prefix "${stage}" --component PowerAppletRuntime
    RESULT_VARIABLE install_status
    OUTPUT_VARIABLE install_output
    ERROR_VARIABLE install_error)
if(NOT install_status EQUAL 0)
    message(FATAL_ERROR
        "Power applet stage install failed:\n${install_output}${install_error}")
endif()

set(shell "${stage}/${QINDAQT_INSTALL_BINDIR}/qindaqt-shell")
set(data "${stage}/${QINDAQT_INSTALL_DATADIR}/qindaqt")
foreach(required_path IN ITEMS
        "${shell}"
        "${data}/applets/power.json"
        "${data}/profiles/qindaqt.json"
        "${data}/themes/qinda-dark.json"
        "${data}/applet-policy/default.json")
    if(NOT EXISTS "${required_path}")
        message(FATAL_ERROR "Power applet stage is missing ${required_path}")
    endif()
endforeach()

# The narrow component is a runnable shell package, so authenticate the new
# direct Controls dependency and its baked sibling Tokens dependency before
# launch. Merely finding either artifact in the stage would not prove that the
# loader follows the installed relative RUNPATH.
unset(ENV{LD_LIBRARY_PATH})
unset(ENV{DYLD_LIBRARY_PATH})
file(GET_RUNTIME_DEPENDENCIES
    EXECUTABLES "${shell}"
    RESOLVED_DEPENDENCIES_VAR resolved_dependencies
    UNRESOLVED_DEPENDENCIES_VAR unresolved_dependencies)
set(resolved_controls_dependency "")
foreach(resolved_dependency IN LISTS resolved_dependencies)
    cmake_path(GET resolved_dependency FILENAME dependency_name)
    if(dependency_name STREQUAL "libqindaqt_controls_qml.so")
        if(NOT resolved_controls_dependency STREQUAL "")
            message(FATAL_ERROR
                "Power applet stage resolved duplicate Controls runtime artifacts")
        endif()
        file(REAL_PATH "${resolved_dependency}" resolved_controls_dependency)
    endif()
endforeach()
if(resolved_controls_dependency STREQUAL "")
    message(FATAL_ERROR "Power applet stage did not resolve QindaQt.Controls")
endif()
cmake_path(IS_PREFIX stage "${resolved_controls_dependency}" NORMALIZE
           controls_is_staged)
if(NOT controls_is_staged)
    message(FATAL_ERROR
        "Power applet Controls dependency escaped its relocated stage: "
        "${resolved_controls_dependency}")
endif()

file(GET_RUNTIME_DEPENDENCIES
    LIBRARIES "${resolved_controls_dependency}"
    RESOLVED_DEPENDENCIES_VAR controls_dependencies
    UNRESOLVED_DEPENDENCIES_VAR controls_unresolved)
set(resolved_tokens_dependency "")
foreach(resolved_dependency IN LISTS controls_dependencies)
    cmake_path(GET resolved_dependency FILENAME dependency_name)
    if(dependency_name STREQUAL "libqindaqt_tokens_qml.so")
        if(NOT resolved_tokens_dependency STREQUAL "")
            message(FATAL_ERROR
                "Power applet stage resolved duplicate Tokens runtime artifacts")
        endif()
        file(REAL_PATH "${resolved_dependency}" resolved_tokens_dependency)
    endif()
endforeach()
set(expected_tokens_dependency "${stage}/Tokens/libqindaqt_tokens_qml.so")
if(NOT EXISTS "${expected_tokens_dependency}")
    message(FATAL_ERROR
        "Power applet stage is missing ${expected_tokens_dependency}")
endif()
file(REAL_PATH "${expected_tokens_dependency}" expected_tokens_dependency)
if(NOT resolved_tokens_dependency STREQUAL expected_tokens_dependency)
    message(FATAL_ERROR
        "Staged Power Controls did not resolve QindaQt.Tokens from its baked "
        "sibling path: expected ${expected_tokens_dependency}, "
        "resolved ${resolved_tokens_dependency}")
endif()

# The module URI and QML type name are compiled into the production binary;
# source-only or manifest-only registrations cannot satisfy this check.
file(STRINGS "${shell}" compiled_qml
     REGEX "(QindaQt\\.Shell\\.PowerApplet|PowerApplet\\.qml)")
if(NOT compiled_qml)
    message(FATAL_ERROR "Staged shell does not contain compiled PowerApplet QML")
endif()

set(poison "${stage}/source-poison")
file(MAKE_DIRECTORY "${poison}/profiles" "${poison}/themes"
                    "${poison}/applets" "${poison}/runtime")
file(WRITE "${poison}/applets/broken.json" "{ not-json")
file(WRITE "${poison}/profiles/broken.json" "{ not-json")
file(WRITE "${poison}/themes/broken.json" "{ not-json")
file(WRITE "${poison}/policy.json" "{ not-json")
file(CHMOD "${poison}/runtime"
     PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)

execute_process(
    COMMAND "${QINDAQT_CMAKE}" -E env
            --unset=LD_LIBRARY_PATH
            --unset=DYLD_LIBRARY_PATH
            QT_QPA_PLATFORM=offscreen
            QT_QUICK_BACKEND=software
            "XDG_RUNTIME_DIR=${poison}/runtime"
            "XDG_DATA_DIRS=${poison}"
            "QINDAQT_PROFILE_DIR=${poison}/profiles"
            "QINDAQT_THEME_DIR=${poison}/themes"
            "QINDAQT_APPLET_DIR=${poison}/applets"
            "QINDAQT_APPLET_POLICY=${poison}/policy.json"
            "${shell}"
            --list
            "--profile-dir=${data}/profiles"
            "--theme-dir=${data}/themes"
            "--applet-dir=${data}/applets"
            "--applet-policy=${data}/applet-policy/default.json"
    RESULT_VARIABLE list_status
    OUTPUT_VARIABLE list_output
    ERROR_VARIABLE list_error)
if(NOT list_status EQUAL 0)
    message(FATAL_ERROR
        "Staged Power applet catalog failed under source poison:\n"
        "${list_output}${list_error}")
endif()
string(FIND "${list_output}" "power - Power" power_entry)
if(power_entry EQUAL -1)
    message(FATAL_ERROR
        "Staged shell did not resolve the installed Power manifest:\n${list_output}")
endif()

message(STATUS
    "Installed compiled Power applet, Controls/Tokens loader paths, and source-poison proof passed")

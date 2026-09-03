# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required IN ITEMS QINDAQT_CMAKE QINDAQT_BUILD_ROOT QINDAQT_STAGE
                          QINDAQT_INSTALL_BINDIR QINDAQT_INSTALL_DATADIR
                          QINDAQT_INSTALL_LIBDIR
                          QINDAQT_KF6_GLOBALACCEL_LIBRARY
                          QINDAQT_KF6_GLOBALACCEL_SONAME
                          QINDAQT_TOKENS_LIBRARY QINDAQT_TOKENS_SONAME)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "Missing installed Audio applet input: ${required}")
    endif()
endforeach()

cmake_path(NORMAL_PATH QINDAQT_BUILD_ROOT OUTPUT_VARIABLE build_root)
cmake_path(NORMAL_PATH QINDAQT_STAGE OUTPUT_VARIABLE stage)
cmake_path(IS_PREFIX build_root "${stage}" NORMALIZE stage_is_in_build)
if(NOT stage_is_in_build OR stage STREQUAL build_root)
    message(FATAL_ERROR "Audio applet stage must be a child of its build tree")
endif()
file(REMOVE_RECURSE "${stage}")

execute_process(
    COMMAND "${QINDAQT_CMAKE}" --install "${build_root}"
            --prefix "${stage}" --component AudioAppletRuntime
    RESULT_VARIABLE install_status
    OUTPUT_VARIABLE install_output
    ERROR_VARIABLE install_error)
if(NOT install_status EQUAL 0)
    message(FATAL_ERROR
            "Audio applet stage install failed:\n${install_output}${install_error}")
endif()

# The production shell links against KF6 GlobalAccel as a platform dependency.
# This isolated stage cannot borrow a machine-wide search path, so provide the
# exact imported artifact selected by the build and let the shell's relative
# install RUNPATH resolve it from the relocated package.
cmake_path(IS_ABSOLUTE QINDAQT_KF6_GLOBALACCEL_LIBRARY kf6_is_absolute)
if(NOT kf6_is_absolute OR NOT EXISTS "${QINDAQT_KF6_GLOBALACCEL_LIBRARY}")
    message(FATAL_ERROR "Audio applet KF6 runtime artifact is invalid")
endif()
if(QINDAQT_KF6_GLOBALACCEL_SONAME MATCHES "[/\\\\]" OR
   QINDAQT_KF6_GLOBALACCEL_SONAME STREQUAL "")
    message(FATAL_ERROR "Audio applet KF6 runtime SONAME is invalid")
endif()
set(stage_library_dir "${stage}/${QINDAQT_INSTALL_LIBDIR}")
file(MAKE_DIRECTORY "${stage_library_dir}")
file(COPY_FILE
     "${QINDAQT_KF6_GLOBALACCEL_LIBRARY}"
     "${stage_library_dir}/${QINDAQT_KF6_GLOBALACCEL_SONAME}"
     ONLY_IF_DIFFERENT)

set(shell "${stage}/${QINDAQT_INSTALL_BINDIR}/qindaqt-shell")
set(data "${stage}/${QINDAQT_INSTALL_DATADIR}/qindaqt")
foreach(required_path IN ITEMS
        "${shell}"
        "${data}/applets/audio.json"
        "${data}/profiles/qindaqt.json"
        "${data}/themes/qinda-dark.json"
        "${data}/applet-policy/default.json")
    if(NOT EXISTS "${required_path}")
        message(FATAL_ERROR "Audio applet stage is missing ${required_path}")
    endif()
endforeach()

# Authenticate the loader path rather than only proving that the process can
# start. With ambient search paths absent, the KF6 SONAME must resolve to the
# exact artifact copied beside this relocated shell.
unset(ENV{LD_LIBRARY_PATH})
unset(ENV{DYLD_LIBRARY_PATH})
file(GET_RUNTIME_DEPENDENCIES
    EXECUTABLES "${shell}"
    RESOLVED_DEPENDENCIES_VAR resolved_dependencies
    UNRESOLVED_DEPENDENCIES_VAR unresolved_dependencies)
set(resolved_kf6_dependency "")
foreach(resolved_dependency IN LISTS resolved_dependencies)
    cmake_path(GET resolved_dependency FILENAME dependency_name)
    if(dependency_name STREQUAL QINDAQT_KF6_GLOBALACCEL_SONAME)
        if(NOT resolved_kf6_dependency STREQUAL "")
            message(FATAL_ERROR
                    "Audio applet stage resolved duplicate KF6 runtime artifacts")
        endif()
        file(REAL_PATH "${resolved_dependency}" resolved_kf6_dependency)
    endif()
endforeach()
file(REAL_PATH
     "${stage_library_dir}/${QINDAQT_KF6_GLOBALACCEL_SONAME}"
     expected_kf6_dependency)
if(NOT resolved_kf6_dependency STREQUAL expected_kf6_dependency)
    message(FATAL_ERROR
            "Staged shell did not resolve KF6 GlobalAccel from its relocated libdir: "
            "expected ${expected_kf6_dependency}, resolved ${resolved_kf6_dependency}")
endif()

# QindaQt.Controls links QindaQt.Tokens with a baked $ORIGIN/../Tokens
# RUNPATH (the sibling QML-module layout built by src/controls), so the
# relocated tokens artifact must sit at that exact sibling path rather than
# in the shell's libdir. Stage it there and authenticate the loader path
# through the staged controls library.
cmake_path(IS_ABSOLUTE QINDAQT_TOKENS_LIBRARY tokens_is_absolute)
if(NOT tokens_is_absolute OR NOT EXISTS "${QINDAQT_TOKENS_LIBRARY}")
    message(FATAL_ERROR "Audio applet Tokens runtime artifact is invalid")
endif()
if(QINDAQT_TOKENS_SONAME MATCHES "[/\\\\]" OR QINDAQT_TOKENS_SONAME STREQUAL "")
    message(FATAL_ERROR "Audio applet Tokens runtime SONAME is invalid")
endif()
set(stage_tokens_dir "${stage}/Tokens")
file(MAKE_DIRECTORY "${stage_tokens_dir}")
file(COPY_FILE
     "${QINDAQT_TOKENS_LIBRARY}"
     "${stage_tokens_dir}/${QINDAQT_TOKENS_SONAME}"
     ONLY_IF_DIFFERENT)
set(controls_library
    "${stage_library_dir}/libqindaqt_controls_qml.so")
if(NOT EXISTS "${controls_library}")
    message(FATAL_ERROR "Audio applet stage is missing ${controls_library}")
endif()
unset(ENV{LD_LIBRARY_PATH})
unset(ENV{DYLD_LIBRARY_PATH})
file(GET_RUNTIME_DEPENDENCIES
    LIBRARIES "${controls_library}"
    RESOLVED_DEPENDENCIES_VAR controls_dependencies
    UNRESOLVED_DEPENDENCIES_VAR controls_unresolved)
set(resolved_tokens_dependency "")
foreach(resolved_dependency IN LISTS controls_dependencies)
    cmake_path(GET resolved_dependency FILENAME dependency_name)
    if(dependency_name STREQUAL QINDAQT_TOKENS_SONAME)
        if(NOT resolved_tokens_dependency STREQUAL "")
            message(FATAL_ERROR
                    "Audio applet stage resolved duplicate Tokens runtime artifacts")
        endif()
        file(REAL_PATH "${resolved_dependency}" resolved_tokens_dependency)
    endif()
endforeach()
file(REAL_PATH
     "${stage_tokens_dir}/${QINDAQT_TOKENS_SONAME}"
     expected_tokens_dependency)
if(NOT resolved_tokens_dependency STREQUAL expected_tokens_dependency)
    message(FATAL_ERROR
            "Staged controls did not resolve QindaQt.Tokens from its baked "
            "sibling path: expected ${expected_tokens_dependency}, "
            "resolved ${resolved_tokens_dependency}")
endif()

file(STRINGS "${shell}" compiled_qml
     REGEX "(QindaQt\\.Shell\\.AudioApplet|AudioApplet\\.qml)")
if(NOT compiled_qml)
    message(FATAL_ERROR "Staged shell does not contain compiled AudioApplet QML")
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
            "Staged Audio applet catalog failed under source poison:\n"
            "${list_output}${list_error}")
endif()
string(FIND "${list_output}" "audio - Audio" audio_entry)
if(audio_entry EQUAL -1)
    message(FATAL_ERROR
            "Staged shell did not resolve the installed Audio manifest:\n${list_output}")
endif()

message(STATUS
    "Installed compiled Audio applet, exact KF6 loader path, and source-poison proof passed")

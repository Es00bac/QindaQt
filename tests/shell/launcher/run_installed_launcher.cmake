# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required IN ITEMS QINDAQT_CMAKE QINDAQT_BUILD_ROOT QINDAQT_STAGE
                          QINDAQT_PROBE QINDAQT_INSTALL_BINDIR
                          QINDAQT_INSTALL_QMLDIR QINDAQT_INSTALL_DATADIR
                          QINDAQT_INSTALL_LIBDIR
                          QINDAQT_KF6_GLOBALACCEL_LIBRARY
                          QINDAQT_KF6_GLOBALACCEL_SONAME
                          QINDAQT_TOKENS_LIBRARY QINDAQT_TOKENS_SONAME)
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
set(relocated_stage "${stage}-relocated")
file(REMOVE_RECURSE "${relocated_stage}")

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
file(RENAME "${stage}" "${relocated_stage}" RESULT relocate_status)
if(NOT relocate_status STREQUAL "0" OR EXISTS "${stage}" OR
   NOT IS_DIRECTORY "${relocated_stage}")
    message(FATAL_ERROR "Launcher stage did not relocate exclusively: ${relocate_status}")
endif()
set(stage "${relocated_stage}")

set(library_dir "${stage}/${QINDAQT_INSTALL_LIBDIR}")
file(MAKE_DIRECTORY "${library_dir}")
cmake_path(IS_ABSOLUTE QINDAQT_KF6_GLOBALACCEL_LIBRARY kf6_is_absolute)
if(NOT kf6_is_absolute OR NOT EXISTS "${QINDAQT_KF6_GLOBALACCEL_LIBRARY}" OR
   QINDAQT_KF6_GLOBALACCEL_SONAME MATCHES "[/\\\\]" OR
   QINDAQT_KF6_GLOBALACCEL_SONAME STREQUAL "")
    message(FATAL_ERROR "Launcher KF6 runtime artifact is invalid")
endif()
file(COPY_FILE "${QINDAQT_KF6_GLOBALACCEL_LIBRARY}"
     "${library_dir}/${QINDAQT_KF6_GLOBALACCEL_SONAME}" ONLY_IF_DIFFERENT)

cmake_path(IS_ABSOLUTE QINDAQT_TOKENS_LIBRARY tokens_is_absolute)
if(NOT tokens_is_absolute OR NOT EXISTS "${QINDAQT_TOKENS_LIBRARY}" OR
   QINDAQT_TOKENS_SONAME MATCHES "[/\\\\]" OR
   QINDAQT_TOKENS_SONAME STREQUAL "")
    message(FATAL_ERROR "Launcher Tokens runtime artifact is invalid")
endif()
set(tokens_dir "${stage}/Tokens")
file(MAKE_DIRECTORY "${tokens_dir}")
file(COPY_FILE "${QINDAQT_TOKENS_LIBRARY}"
     "${tokens_dir}/${QINDAQT_TOKENS_SONAME}" ONLY_IF_DIFFERENT)

set(shell "${stage}/${QINDAQT_INSTALL_BINDIR}/qindaqt-shell")
set(data "${stage}/${QINDAQT_INSTALL_DATADIR}/qindaqt")
set(qml "${stage}/${QINDAQT_INSTALL_QMLDIR}")
set(controls "${library_dir}/libqindaqt_controls_qml.so")
set(launcher_library "${library_dir}/libqindaqt_shell_launcher_qml.so")
foreach(required_path IN ITEMS
        "${shell}" "${controls}" "${launcher_library}"
        "${qml}/QindaQt/Shell/Launcher/qmldir"
        "${qml}/QindaQt/Shell/Launcher/LauncherApplet.qml"
        "${data}/applets/launcher.json"
        "${data}/profiles/qindaqt.json"
        "${data}/themes/qinda-dark.json"
        "${data}/applet-policy/default.json")
    if(NOT EXISTS "${required_path}")
        message(FATAL_ERROR "Launcher stage is missing ${required_path}")
    endif()
endforeach()
file(GLOB launcher_plugins
    "${qml}/QindaQt/Shell/Launcher/*qindaqt_shell_launcher*plugin*")
if(NOT launcher_plugins)
    message(FATAL_ERROR "Launcher stage contains no compiled QML plugin")
endif()

unset(ENV{LD_LIBRARY_PATH})
unset(ENV{DYLD_LIBRARY_PATH})
file(GET_RUNTIME_DEPENDENCIES
    EXECUTABLES "${shell}"
    RESOLVED_DEPENDENCIES_VAR shell_dependencies
    UNRESOLVED_DEPENDENCIES_VAR shell_unresolved)
foreach(expected_pair IN ITEMS
        "${QINDAQT_KF6_GLOBALACCEL_SONAME}|${library_dir}/${QINDAQT_KF6_GLOBALACCEL_SONAME}"
        "libqindaqt_shell_launcher_qml.so|${launcher_library}")
    string(REPLACE "|" ";" pair "${expected_pair}")
    list(GET pair 0 soname)
    list(GET pair 1 expected_path)
    set(resolved_path "")
    foreach(dependency IN LISTS shell_dependencies)
        cmake_path(GET dependency FILENAME dependency_name)
        if(dependency_name STREQUAL soname)
            if(NOT resolved_path STREQUAL "")
                message(FATAL_ERROR "Launcher stage resolved duplicate ${soname}")
            endif()
            file(REAL_PATH "${dependency}" resolved_path)
        endif()
    endforeach()
    file(REAL_PATH "${expected_path}" expected_real_path)
    if(NOT resolved_path STREQUAL expected_real_path)
        message(FATAL_ERROR
            "Launcher stage did not resolve ${soname} from ${expected_real_path}; "
            "resolved ${resolved_path}")
    endif()
endforeach()

file(GET_RUNTIME_DEPENDENCIES
    LIBRARIES "${controls}"
    RESOLVED_DEPENDENCIES_VAR controls_dependencies
    UNRESOLVED_DEPENDENCIES_VAR controls_unresolved)
set(resolved_tokens "")
foreach(dependency IN LISTS controls_dependencies)
    cmake_path(GET dependency FILENAME dependency_name)
    if(dependency_name STREQUAL QINDAQT_TOKENS_SONAME)
        file(REAL_PATH "${dependency}" resolved_tokens)
    endif()
endforeach()
file(REAL_PATH "${tokens_dir}/${QINDAQT_TOKENS_SONAME}" expected_tokens)
if(NOT resolved_tokens STREQUAL expected_tokens)
    message(FATAL_ERROR
        "Launcher Controls did not resolve staged Tokens: expected "
        "${expected_tokens}, resolved ${resolved_tokens}")
endif()

file(STRINGS "${launcher_library}" compiled_qml
     REGEX "(QindaQt\\.Shell\\.Launcher|LauncherApplet\\.qml)")
if(NOT compiled_qml)
    message(FATAL_ERROR "Launcher backing library contains no compiled QML evidence")
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
            --unset=LD_LIBRARY_PATH --unset=DYLD_LIBRARY_PATH
            --unset=DBUS_SESSION_BUS_ADDRESS --unset=DISPLAY
            --unset=WAYLAND_DISPLAY
            QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software
            "XDG_RUNTIME_DIR=${poison}/runtime" "XDG_DATA_DIRS=${poison}"
            "XDG_DATA_HOME=${poison}"
            "QINDAQT_PROFILE_DIR=${poison}/profiles"
            "QINDAQT_THEME_DIR=${poison}/themes"
            "QINDAQT_APPLET_DIR=${poison}/applets"
            "QINDAQT_APPLET_POLICY=${poison}/policy.json"
            "${shell}" --list
            "--profile-dir=${data}/profiles" "--theme-dir=${data}/themes"
            "--applet-dir=${data}/applets"
            "--applet-policy=${data}/applet-policy/default.json"
    RESULT_VARIABLE list_status
    OUTPUT_VARIABLE list_output
    ERROR_VARIABLE list_error)
if(NOT list_status EQUAL 0 OR NOT list_output MATCHES
   "launcher - Application Launcher")
    message(FATAL_ERROR
        "Installed launcher shell failed under source poison:\n"
        "${list_output}${list_error}")
endif()

execute_process(
    COMMAND "${QINDAQT_CMAKE}" -E env
            --unset=DBUS_SESSION_BUS_ADDRESS --unset=DISPLAY
            --unset=WAYLAND_DISPLAY
            QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software
            QT_FATAL_WARNINGS=1 QML_IMPORT_TRACE=1
            "XDG_RUNTIME_DIR=${poison}/runtime"
            "XDG_DATA_DIRS=${poison}" "XDG_DATA_HOME=${poison}"
            "${QINDAQT_PROBE}"
            "--applets-dir=${data}/applets"
            "--policy=${data}/applet-policy/default.json"
            "--staged-qml=${qml}"
            "--theme=${data}/themes/qinda-dark.json"
    RESULT_VARIABLE probe_status
    OUTPUT_VARIABLE probe_output
    ERROR_VARIABLE probe_error)
if(NOT probe_status EQUAL 0 OR
   NOT "${probe_output}${probe_error}" MATCHES "${qml}")
    message(FATAL_ERROR
        "Installed launcher QML probe failed:\n${probe_output}${probe_error}")
endif()

message(STATUS
    "Relocated production Launcher shell, QML, loader closure, and source poison passed")

# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required IN ITEMS QINDAQT_CMAKE QINDAQT_BUILD_ROOT QINDAQT_STAGE
                          QINDAQT_INSTALL_BINDIR QINDAQT_INSTALL_DATADIR
                          QINDAQT_INSTALL_LIBDIR QINDAQT_INSTALL_QMLDIR)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "Missing installed Global Menu input: ${required}")
    endif()
endforeach()

cmake_path(NORMAL_PATH QINDAQT_BUILD_ROOT OUTPUT_VARIABLE build_root)
cmake_path(NORMAL_PATH QINDAQT_STAGE OUTPUT_VARIABLE stage)
cmake_path(IS_PREFIX build_root "${stage}" NORMALIZE stage_is_in_build)
if(NOT stage_is_in_build OR stage STREQUAL build_root)
    message(FATAL_ERROR "Global Menu stage must be a child of its build tree")
endif()
file(REMOVE_RECURSE "${stage}")
execute_process(
    COMMAND "${QINDAQT_CMAKE}" --install "${build_root}"
            --prefix "${stage}" --component GlobalMenuAppletRuntime
    RESULT_VARIABLE install_status
    OUTPUT_VARIABLE install_output
    ERROR_VARIABLE install_error)
if(NOT install_status EQUAL 0)
    message(FATAL_ERROR
            "Global Menu stage install failed:\n${install_output}${install_error}")
endif()

set(shell "${stage}/${QINDAQT_INSTALL_BINDIR}/qindaqt-shell")
set(libdir "${stage}/${QINDAQT_INSTALL_LIBDIR}")
set(data "${stage}/${QINDAQT_INSTALL_DATADIR}/qindaqt")
set(qml "${stage}/${QINDAQT_INSTALL_QMLDIR}/QindaQt/Shell/GlobalMenu")
foreach(required_path IN ITEMS
        "${shell}"
        "${libdir}/libqindaqt_global_menu_qml.so"
        "${qml}/libqindaqt_global_menu_qml.so"
        "${qml}/libqindaqt_global_menu_qmlplugin.so"
        "${qml}/qmldir"
        "${qml}/GlobalMenuApplet.qml"
        "${qml}/GlobalMenuPopup.qml"
        "${data}/applets/global-menu.json"
        "${data}/profiles/qindaqt.json"
        "${data}/themes/qinda-dark.json"
        "${data}/applet-policy/default.json")
    if(NOT EXISTS "${required_path}")
        message(FATAL_ERROR "Global Menu stage is missing ${required_path}")
    endif()
endforeach()

unset(ENV{LD_LIBRARY_PATH})
unset(ENV{DYLD_LIBRARY_PATH})
file(GET_RUNTIME_DEPENDENCIES
    EXECUTABLES "${shell}"
    RESOLVED_DEPENDENCIES_VAR resolved_dependencies
    UNRESOLVED_DEPENDENCIES_VAR unresolved_dependencies)
set(resolved_global_menu "")
foreach(dependency IN LISTS resolved_dependencies)
    cmake_path(GET dependency FILENAME dependency_name)
    if(dependency_name STREQUAL "libqindaqt_global_menu_qml.so")
        file(REAL_PATH "${dependency}" resolved_global_menu)
    endif()
endforeach()
file(REAL_PATH "${libdir}/libqindaqt_global_menu_qml.so"
     expected_global_menu)
if(NOT resolved_global_menu STREQUAL expected_global_menu)
    message(FATAL_ERROR
            "Staged shell did not resolve its Global Menu QML library from the package")
endif()

file(STRINGS "${qml}/libqindaqt_global_menu_qml.so" compiled_qml
     REGEX "(QindaQt\\.Shell\\.GlobalMenu|GlobalMenuPopup\\.qml)")
if(NOT compiled_qml)
    message(FATAL_ERROR "Installed Global Menu library has no compiled popup QML")
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
            QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software
            "XDG_RUNTIME_DIR=${poison}/runtime"
            "XDG_DATA_DIRS=${poison}"
            "QINDAQT_PROFILE_DIR=${poison}/profiles"
            "QINDAQT_THEME_DIR=${poison}/themes"
            "QINDAQT_APPLET_DIR=${poison}/applets"
            "QINDAQT_APPLET_POLICY=${poison}/policy.json"
            "${shell}" --list
            "--profile-dir=${data}/profiles"
            "--theme-dir=${data}/themes"
            "--applet-dir=${data}/applets"
            "--applet-policy=${data}/applet-policy/default.json"
    RESULT_VARIABLE list_status
    OUTPUT_VARIABLE list_output
    ERROR_VARIABLE list_error)
if(NOT list_status EQUAL 0)
    message(FATAL_ERROR
            "Installed Global Menu catalog failed under source poison:\n"
            "${list_output}${list_error}")
endif()
string(FIND "${list_output}" "global-menu - Global Menu" global_menu_entry)
if(global_menu_entry EQUAL -1)
    message(FATAL_ERROR
            "Installed shell did not resolve Global Menu:\n${list_output}")
endif()

message(STATUS
    "Installed compiled Global Menu, exact loader path, and source-poison proof passed")

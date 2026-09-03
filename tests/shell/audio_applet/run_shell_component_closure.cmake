# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required IN ITEMS QINDAQT_CMAKE QINDAQT_SHELL_BUILD_ROOT
                          QINDAQT_SHELL_CMAKE QINDAQT_SHELL_INSTALL_CMAKE
                          QINDAQT_STAGE_ROOT
                          QINDAQT_INSTALL_BINDIR QINDAQT_INSTALL_LIBDIR)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "Missing shell component-closure input: ${required}")
    endif()
endforeach()

cmake_path(NORMAL_PATH QINDAQT_SHELL_BUILD_ROOT OUTPUT_VARIABLE shell_build_root)
cmake_path(NORMAL_PATH QINDAQT_STAGE_ROOT OUTPUT_VARIABLE stage_root)
cmake_path(GET shell_build_root PARENT_PATH build_src_root)
cmake_path(GET build_src_root PARENT_PATH build_root)
cmake_path(IS_PREFIX build_root "${stage_root}" NORMALIZE stage_is_in_build)
if(NOT stage_is_in_build OR stage_root STREQUAL build_root)
    message(FATAL_ERROR "Shell component stages must be children of the build tree")
endif()

# AGENT-GUARD: This is the complete component inventory for install rules that
# carry qindaqt-shell in the shell CMake boundary. Add every future
# shell-carrying component and owning install module here so its isolated
# runtime closure cannot regress.
set(shell_components
    QindaQt
    AudioAppletRuntime
    BluetoothAppletRuntime
    ClipboardAppletRuntime
    GlobalMenuAppletRuntime
    LauncherAppletRuntime
    PowerAppletRuntime
    TaskListAppletRuntime)

# Keep the executable inventory closed as well as testing the known stages.
# An added shell install rule must extend the executable cases below instead
# of silently escaping component-isolation coverage.
file(READ "${QINDAQT_SHELL_CMAKE}" shell_cmake)
file(READ "${QINDAQT_SHELL_INSTALL_CMAKE}" shell_install_cmake)
string(APPEND shell_cmake "\n${shell_install_cmake}")
string(REGEX MATCHALL
    "install\\([ \t\r\n]*TARGETS[ \t\r\n]+qindaqt-shell[ \t\r\n][^\\)]*\\)"
    shell_install_rules "${shell_cmake}")
set(declared_shell_components "")
foreach(install_rule IN LISTS shell_install_rules)
    if(NOT install_rule MATCHES
       "COMPONENT[ \t\r\n]+([A-Za-z][A-Za-z0-9_]*)")
        message(FATAL_ERROR
            "Every qindaqt-shell install rule must name its component explicitly")
    endif()
    list(APPEND declared_shell_components "${CMAKE_MATCH_1}")
endforeach()
list(SORT declared_shell_components)
set(expected_shell_components ${shell_components})
list(SORT expected_shell_components)
if(NOT declared_shell_components STREQUAL expected_shell_components)
    message(FATAL_ERROR
        "Shell-carrying install component inventory changed: declared "
        "${declared_shell_components}, tested ${expected_shell_components}")
endif()

file(REMOVE_RECURSE "${stage_root}")
foreach(component IN LISTS shell_components)
    set(stage "${stage_root}/${component}")
    execute_process(
        COMMAND "${QINDAQT_CMAKE}" --install "${shell_build_root}"
                --prefix "${stage}" --component "${component}"
        RESULT_VARIABLE install_status
        OUTPUT_VARIABLE install_output
        ERROR_VARIABLE install_error)
    if(NOT install_status EQUAL 0)
        message(FATAL_ERROR
            "${component} shell stage install failed:\n"
            "${install_output}${install_error}")
    endif()

    set(shell "${stage}/${QINDAQT_INSTALL_BINDIR}/qindaqt-shell")
    set(controls
        "${stage}/${QINDAQT_INSTALL_LIBDIR}/libqindaqt_controls_qml.so")
    set(tokens "${stage}/Tokens/libqindaqt_tokens_qml.so")
    set(launcher
        "${stage}/${QINDAQT_INSTALL_LIBDIR}/libqindaqt_shell_launcher_qml.so")
    set(global_menu
        "${stage}/${QINDAQT_INSTALL_LIBDIR}/libqindaqt_global_menu_qml.so")
    set(clipboard_module
        "${stage}/${QINDAQT_INSTALL_LIBDIR}/qt6/qml/QindaQt/Shell/ClipboardApplet")
    foreach(required_path IN ITEMS "${shell}" "${controls}" "${tokens}"
                                   "${launcher}" "${global_menu}"
                                   "${clipboard_module}/qmldir"
                                   "${clipboard_module}/qml/ClipboardApplet.qml"
                                   "${clipboard_module}/qml/ClipboardEntryRow.qml"
                                   "${clipboard_module}/qml/ClipboardPanelApplet.qml")
        if(NOT EXISTS "${required_path}")
            message(FATAL_ERROR
                "${component} shell stage is missing ${required_path}")
        endif()
    endforeach()

    unset(ENV{LD_LIBRARY_PATH})
    unset(ENV{DYLD_LIBRARY_PATH})
    file(GET_RUNTIME_DEPENDENCIES
        EXECUTABLES "${shell}"
        RESOLVED_DEPENDENCIES_VAR shell_dependencies
        UNRESOLVED_DEPENDENCIES_VAR shell_unresolved)
    set(resolved_controls "")
    foreach(dependency IN LISTS shell_dependencies)
        cmake_path(GET dependency FILENAME dependency_name)
        if(dependency_name STREQUAL "libqindaqt_controls_qml.so")
            if(NOT resolved_controls STREQUAL "")
                message(FATAL_ERROR
                    "${component} shell resolved duplicate Controls libraries")
            endif()
            file(REAL_PATH "${dependency}" resolved_controls)
        endif()
    endforeach()
    file(REAL_PATH "${controls}" expected_controls)
    if(NOT resolved_controls STREQUAL expected_controls)
        message(FATAL_ERROR
            "${component} shell did not resolve staged Controls: expected "
            "${expected_controls}, resolved ${resolved_controls}")
    endif()

    set(resolved_launcher "")
    foreach(dependency IN LISTS shell_dependencies)
        cmake_path(GET dependency FILENAME dependency_name)
        if(dependency_name STREQUAL "libqindaqt_shell_launcher_qml.so")
            if(NOT resolved_launcher STREQUAL "")
                message(FATAL_ERROR
                    "${component} shell resolved duplicate Launcher libraries")
            endif()
            file(REAL_PATH "${dependency}" resolved_launcher)
        endif()
    endforeach()
    file(REAL_PATH "${launcher}" expected_launcher)
    if(NOT resolved_launcher STREQUAL expected_launcher)
        message(FATAL_ERROR
            "${component} shell did not resolve staged Launcher: expected "
            "${expected_launcher}, resolved ${resolved_launcher}")
    endif()

    set(resolved_global_menu "")
    foreach(dependency IN LISTS shell_dependencies)
        cmake_path(GET dependency FILENAME dependency_name)
        if(dependency_name STREQUAL "libqindaqt_global_menu_qml.so")
            if(NOT resolved_global_menu STREQUAL "")
                message(FATAL_ERROR
                    "${component} shell resolved duplicate Global Menu libraries")
            endif()
            file(REAL_PATH "${dependency}" resolved_global_menu)
        endif()
    endforeach()
    file(REAL_PATH "${global_menu}" expected_global_menu)
    if(NOT resolved_global_menu STREQUAL expected_global_menu)
        message(FATAL_ERROR
            "${component} shell did not resolve staged Global Menu: expected "
            "${expected_global_menu}, resolved ${resolved_global_menu}")
    endif()

    file(GET_RUNTIME_DEPENDENCIES
        LIBRARIES "${controls}"
        RESOLVED_DEPENDENCIES_VAR controls_dependencies
        UNRESOLVED_DEPENDENCIES_VAR controls_unresolved)
    set(resolved_tokens "")
    foreach(dependency IN LISTS controls_dependencies)
        cmake_path(GET dependency FILENAME dependency_name)
        if(dependency_name STREQUAL "libqindaqt_tokens_qml.so")
            if(NOT resolved_tokens STREQUAL "")
                message(FATAL_ERROR
                    "${component} Controls resolved duplicate Tokens libraries")
            endif()
            file(REAL_PATH "${dependency}" resolved_tokens)
        endif()
    endforeach()
    file(REAL_PATH "${tokens}" expected_tokens)
    if(NOT resolved_tokens STREQUAL expected_tokens)
        message(FATAL_ERROR
            "${component} Controls did not resolve staged Tokens: expected "
            "${expected_tokens}, resolved ${resolved_tokens}")
    endif()

    execute_process(
        COMMAND "${QINDAQT_CMAKE}" -E env
                --unset=LD_LIBRARY_PATH
                --unset=DYLD_LIBRARY_PATH
                --unset=DBUS_SESSION_BUS_ADDRESS
                --unset=DISPLAY
                --unset=WAYLAND_DISPLAY
                "${shell}" --help
        RESULT_VARIABLE shell_status
        OUTPUT_VARIABLE shell_output
        ERROR_VARIABLE shell_error)
    if(NOT shell_status EQUAL 0)
        message(FATAL_ERROR
            "${component} staged shell failed with ambient runtime variables "
            "cleared:\n${shell_output}${shell_error}")
    endif()
endforeach()

message(STATUS
    "Every shell-carrying install component has runnable Clipboard/GlobalMenu/Launcher/Controls/Tokens closure")

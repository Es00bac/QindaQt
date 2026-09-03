# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required IN ITEMS QINDAQT_CMAKE QINDAQT_BUILD_ROOT QINDAQT_STAGE
                          QINDAQT_INSTALL_BINDIR QINDAQT_INSTALL_DATADIR
                          QINDAQT_INSTALL_LIBDIR QINDAQT_INSTALL_QMLDIR)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "Missing installed Clipboard runtime input: ${required}")
    endif()
endforeach()

cmake_path(NORMAL_PATH QINDAQT_BUILD_ROOT OUTPUT_VARIABLE build_root)
cmake_path(NORMAL_PATH QINDAQT_STAGE OUTPUT_VARIABLE stage)
cmake_path(IS_PREFIX build_root "${stage}" NORMALIZE stage_is_in_build)
if(NOT stage_is_in_build OR stage STREQUAL build_root)
    message(FATAL_ERROR "Clipboard runtime stage must be a child of the build tree")
endif()

file(REMOVE_RECURSE "${stage}")
execute_process(
    COMMAND "${QINDAQT_CMAKE}" --install "${build_root}"
            --prefix "${stage}" --component ClipboardAppletRuntime
    RESULT_VARIABLE install_status
    OUTPUT_VARIABLE install_output
    ERROR_VARIABLE install_error)
if(NOT install_status EQUAL 0)
    message(FATAL_ERROR
        "ClipboardAppletRuntime install failed:\n${install_output}${install_error}")
endif()

set(shell "${stage}/${QINDAQT_INSTALL_BINDIR}/qindaqt-shell")
set(data "${stage}/${QINDAQT_INSTALL_DATADIR}/qindaqt")
set(module "${stage}/${QINDAQT_INSTALL_QMLDIR}/QindaQt/Shell/ClipboardApplet")
foreach(required_path IN ITEMS
        "${shell}"
        "${data}/applets/clipboard.json"
        "${data}/profiles/qindaqt.json"
        "${data}/themes/qinda-dark.json"
        "${data}/applet-policy/default.json"
        "${module}/qmldir"
        "${module}/qindaqt_shell_clipboard_applet.qmltypes"
        "${module}/qml/ClipboardApplet.qml"
        "${module}/qml/ClipboardEntryRow.qml"
        "${module}/qml/ClipboardPanelApplet.qml"
        "${module}/libqindaqt_shell_clipboard_applet_runtime.a"
        "${module}/libqindaqt_shell_clipboard_applet_runtimeplugin.a"
        "${stage}/${QINDAQT_INSTALL_LIBDIR}/libqindaqt_controls_qml.so"
        "${stage}/Tokens/libqindaqt_tokens_qml.so"
        "${stage}/${QINDAQT_INSTALL_LIBDIR}/libqindaqt_shell_launcher_qml.so"
        "${stage}/${QINDAQT_INSTALL_LIBDIR}/libqindaqt_global_menu_qml.so")
    if(NOT EXISTS "${required_path}")
        message(FATAL_ERROR
            "ClipboardAppletRuntime stage is missing ${required_path}")
    endif()
endforeach()

file(STRINGS "${shell}" compiled_boundary
     REGEX "(ClipboardPanelApplet|services.clipboardHistory|org.qindaqt.Clipboard1)")
if(NOT compiled_boundary)
    message(FATAL_ERROR
        "Staged shell contains no compiled Clipboard composition evidence")
endif()

set(poison "${stage}/source-poison")
file(MAKE_DIRECTORY "${poison}/profiles" "${poison}/themes"
                    "${poison}/applets" "${poison}/runtime")
file(WRITE "${poison}/applets/broken.json" "{ not-json")
file(WRITE "${poison}/profiles/broken.json" "{ not-json")
file(WRITE "${poison}/themes/broken.json" "{ not-json")
file(WRITE "${poison}/policy.json" "{ not-json")

execute_process(
    COMMAND "${QINDAQT_CMAKE}" -E env
            --unset=LD_LIBRARY_PATH --unset=DYLD_LIBRARY_PATH
            --unset=DBUS_SESSION_BUS_ADDRESS --unset=DISPLAY
            --unset=WAYLAND_DISPLAY
            QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software
            "XDG_RUNTIME_DIR=${poison}/runtime"
            "XDG_DATA_DIRS=${poison}" "XDG_DATA_HOME=${poison}"
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
        "Staged Clipboard shell failed under source poison:\n"
        "${list_output}${list_error}")
endif()
string(FIND "${list_output}" "clipboard - Clipboard" clipboard_entry)
if(clipboard_entry EQUAL -1)
    message(FATAL_ERROR
        "Staged shell did not resolve Clipboard:\n${list_output}")
endif()

message(STATUS
    "Installed Clipboard production shell, compiled module, and source-poison proof passed")

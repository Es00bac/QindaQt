# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required IN ITEMS QINDAQT_CMAKE QINDAQT_BUILD_ROOT QINDAQT_STAGE
                          QINDAQT_INSTALL_BINDIR QINDAQT_INSTALL_DATADIR)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "Missing installed Bluetooth applet input: ${required}")
    endif()
endforeach()

cmake_path(NORMAL_PATH QINDAQT_BUILD_ROOT OUTPUT_VARIABLE build_root)
cmake_path(NORMAL_PATH QINDAQT_STAGE OUTPUT_VARIABLE stage)
cmake_path(IS_PREFIX build_root "${stage}" NORMALIZE stage_is_in_build)
if(NOT stage_is_in_build OR stage STREQUAL build_root)
    message(FATAL_ERROR "Bluetooth applet stage must be a child of its build tree")
endif()
file(REMOVE_RECURSE "${stage}")

execute_process(
    COMMAND "${QINDAQT_CMAKE}" --install "${build_root}"
            --prefix "${stage}" --component BluetoothAppletRuntime
    RESULT_VARIABLE install_status
    OUTPUT_VARIABLE install_output
    ERROR_VARIABLE install_error)
if(NOT install_status EQUAL 0)
    message(FATAL_ERROR
        "Bluetooth applet stage install failed:\n${install_output}${install_error}")
endif()

set(shell "${stage}/${QINDAQT_INSTALL_BINDIR}/qindaqt-shell")
set(data "${stage}/${QINDAQT_INSTALL_DATADIR}/qindaqt")
foreach(required_path IN ITEMS
        "${shell}"
        "${data}/applets/bluetooth.json"
        "${data}/profiles/qindaqt.json"
        "${data}/themes/qinda-dark.json"
        "${data}/applet-policy/default.json")
    if(NOT EXISTS "${required_path}")
        message(FATAL_ERROR "Bluetooth applet stage is missing ${required_path}")
    endif()
endforeach()

file(STRINGS "${shell}" compiled_qml
     REGEX "(QindaQt\\.Shell\\.BluetoothApplet|BluetoothApplet\\.qml)")
if(NOT compiled_qml)
    message(FATAL_ERROR "Staged shell does not contain compiled BluetoothApplet QML")
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
        "Staged Bluetooth applet catalog failed under source poison:\n"
        "${list_output}${list_error}")
endif()
string(FIND "${list_output}" "bluetooth - Bluetooth" bluetooth_entry)
if(bluetooth_entry EQUAL -1)
    message(FATAL_ERROR
        "Staged shell did not resolve the installed Bluetooth manifest:\n${list_output}")
endif()

message(STATUS "Installed compiled Bluetooth applet and source-poison proof passed")

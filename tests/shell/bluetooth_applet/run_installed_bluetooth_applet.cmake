# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required IN ITEMS QINDAQT_CMAKE QINDAQT_BUILD_ROOT QINDAQT_STAGE
                          QINDAQT_INSTALL_BINDIR QINDAQT_INSTALL_DATADIR
                          QINDAQT_INSTALL_LIBDIR
                          QINDAQT_KF6_GLOBALACCEL_LIBRARY
                          QINDAQT_KF6_GLOBALACCEL_SONAME)
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

# The production shell links against KF6 GlobalAccel as a platform dependency.
# This isolated stage cannot borrow a machine-wide search path, so provide the
# exact imported artifact selected by the build and let the shell's relative
# install RUNPATH resolve it from the relocated package.
cmake_path(IS_ABSOLUTE QINDAQT_KF6_GLOBALACCEL_LIBRARY kf6_is_absolute)
if(NOT kf6_is_absolute OR NOT EXISTS "${QINDAQT_KF6_GLOBALACCEL_LIBRARY}")
    message(FATAL_ERROR "Bluetooth applet KF6 runtime artifact is invalid")
endif()
if(QINDAQT_KF6_GLOBALACCEL_SONAME MATCHES "[/\\\\]" OR
   QINDAQT_KF6_GLOBALACCEL_SONAME STREQUAL "")
    message(FATAL_ERROR "Bluetooth applet KF6 runtime SONAME is invalid")
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
        "Staged Bluetooth applet catalog failed under source poison:\n"
        "${list_output}${list_error}")
endif()
string(FIND "${list_output}" "bluetooth - Bluetooth" bluetooth_entry)
if(bluetooth_entry EQUAL -1)
    message(FATAL_ERROR
        "Staged shell did not resolve the installed Bluetooth manifest:\n${list_output}")
endif()

message(STATUS "Installed compiled Bluetooth applet and source-poison proof passed")

# SPDX-License-Identifier: GPL-3.0-or-later

add_test(
    NAME desktop.virtual.package-contract
    COMMAND
        "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/test_desktop_session_package.py"
        --cmake "${CMAKE_COMMAND}"
        --build-root "${CMAKE_BINARY_DIR}"
        ${_qindaqt_desktop_common_arguments}
        --qml-directory "${QT6_INSTALL_QML}"
        --network-qml-library
        "$<TARGET_FILE_NAME:qindaqt_settings_network_qml>"
        --network-qml-plugin
        "$<TARGET_FILE_NAME:qindaqt_settings_network_qmlplugin>"
        --configuration "$<CONFIG>"
)
set_tests_properties(
    desktop.virtual.package-contract
    PROPERTIES
        TIMEOUT 150
        RUN_SERIAL TRUE
        FIXTURES_SETUP desktop_virtual_stage
        LABELS "integration;install;session;display"
)

set(_qindaqt_desktop_system_library_arguments)
foreach(_system_directory IN LISTS CMAKE_CXX_IMPLICIT_LINK_DIRECTORIES)
    if(IS_ABSOLUTE "${_system_directory}" AND EXISTS "${_system_directory}")
        list(
            APPEND _qindaqt_desktop_system_library_arguments
            --system-library-directory "${_system_directory}"
        )
    endif()
endforeach()
foreach(_package_directory IN ITEMS
        "${KWin_DIR}" "${KDecoration3_DIR}" "${KF6GlobalAccel_DIR}")
    cmake_path(GET _package_directory PARENT_PATH _package_cmake_directory)
    cmake_path(GET _package_cmake_directory PARENT_PATH _package_library_directory)
    if(EXISTS "${_package_library_directory}")
        list(
            APPEND _qindaqt_desktop_system_library_arguments
            --system-library-directory "${_package_library_directory}"
        )
    endif()
endforeach()
set(
    _qindaqt_desktop_stage_closure
    "${CMAKE_CURRENT_BINARY_DIR}/desktop-stage-closure-stage"
)
add_test(
    NAME desktop.virtual.stage-closure
    COMMAND
        "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/test_desktop_session_stage_closure.py"
        --cmake "${CMAKE_COMMAND}"
        --readelf "${QINDAQT_READELF}"
        --build-root "${CMAKE_BINARY_DIR}"
        --source-root "${PROJECT_SOURCE_DIR}"
        --stage-root "${_qindaqt_desktop_stage_closure}"
        --bin-directory "${CMAKE_INSTALL_BINDIR}"
        --lib-directory "${CMAKE_INSTALL_LIBDIR}"
        --qml-directory "${QT6_INSTALL_QML}"
        --qml-source "${PROJECT_SOURCE_DIR}/src/shell/qml/BuiltinAppletContent.qml"
        --qml-source "${PROJECT_SOURCE_DIR}/src/shell/qml/AppletChip.qml"
        --qml-source "${PROJECT_SOURCE_DIR}/src/shell/qml/PanelAppletRow.qml"
        --qml-source "${PROJECT_SOURCE_DIR}/src/shell/qml/PanelAppletColumn.qml"
        --qml-source "${PROJECT_SOURCE_DIR}/src/shell/qml/PanelContent.qml"
        --qml-source "${PROJECT_SOURCE_DIR}/src/shell/qml/RuntimePanel.qml"
        # AGENT-NOTE: Ingrid Daubechies P1 on rejected candidate 99b0619:
        # omitting SettingsApp/Main.qml let this row pass while its staged
        # Audio, Bluetooth, and Power route imports were unloadable.
        --qml-source "${PROJECT_SOURCE_DIR}/src/apps/settings_center/Main.qml"
        --embedded-qml-module QindaQt.SettingsApp.Customize
        --embedded-qml-module QindaQt.SettingsApp.PowerBackend
        # AGENT-NOTE: ColorBackend is also a static module linked into the
        # Settings Center (src/apps/settings/color/CMakeLists.txt), so the
        # stage carries no qmldir for it; exempt it like PowerBackend.
        --embedded-qml-module QindaQt.SettingsApp.ColorBackend
        ${_qindaqt_desktop_system_library_arguments}
        --required-shell-library "$<TARGET_FILE_NAME:qindaqt_controls_qml>"
        --required-shell-library "$<TARGET_FILE_NAME:qindaqt_global_menu_qml>"
        --required-shell-library "$<TARGET_FILE_NAME:qindaqt_shell_launcher_qml>"
        --negative-library "$<TARGET_FILE_NAME:qindaqt_global_menu_qml>"
        --negative-qml-module QindaQt.SettingsApp.Power
        --configuration "$<CONFIG>"
)
set_tests_properties(
    desktop.virtual.stage-closure
    PROPERTIES
        TIMEOUT 150
        RUN_SERIAL TRUE
        ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1"
        LABELS "integration;install;session;security"
)

unset(_qindaqt_desktop_stage_closure)
unset(_qindaqt_desktop_system_library_arguments)
unset(_package_cmake_directory)
unset(_package_directory)
unset(_package_library_directory)
unset(_system_directory)

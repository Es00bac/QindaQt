# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required IN ITEMS QINDAQT_CMAKE QINDAQT_BUILD_DIRECTORY
        QINDAQT_INSTALL_PREFIX QINDAQT_INSTALL_INCLUDEDIR QINDAQT_INSTALL_LIBDIR
        QINDAQT_INSTALL_LIBEXECDIR QINDAQT_INSTALL_DBUSSERVICEDIR
        QINDAQT_INSTALL_SYSTEMDUSERUNITDIR QINDAQT_INSTALL_DATADIR
        QINDAQT_EXPECTED_PORTAL_EXECUTABLE QINDAQT_EXPECTED_SETTINGS_EXECUTABLE
        QINDAQT_EXPECTED_SCHEMA_DIR QINDAQT_PROCESS_TEST CHECK_SCRIPT
        QINDAQT_PROTECTED_CAPTURE_AVAILABLE QINDAQT_CAPTURE_AUTHORITY_PATHS
        QINDAQT_EXPECTED_CAPTURE_LIBEXEC QINDAQT_EXPECTED_CAPTURE_DATA
        SOURCE_PORTAL_ROOT)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "Missing staged portal package input: ${required}")
    endif()
endforeach()

cmake_path(NORMAL_PATH QINDAQT_BUILD_DIRECTORY OUTPUT_VARIABLE build_directory)
cmake_path(NORMAL_PATH QINDAQT_INSTALL_PREFIX OUTPUT_VARIABLE install_prefix)
cmake_path(IS_PREFIX build_directory "${install_prefix}" NORMALIZE prefix_is_in_build)
if(NOT prefix_is_in_build OR install_prefix STREQUAL build_directory)
    message(FATAL_ERROR "Refusing to replace a portal stage outside its build tree")
endif()

file(REMOVE_RECURSE "${install_prefix}")
foreach(component IN ITEMS QindaQtPortalP0)
    set(install_command "${QINDAQT_CMAKE}" --install "${build_directory}"
        --prefix "${install_prefix}" --component "${component}")
    if(DEFINED QINDAQT_CONFIGURATION AND NOT QINDAQT_CONFIGURATION STREQUAL "")
        list(APPEND install_command --config "${QINDAQT_CONFIGURATION}")
    endif()
    execute_process(
        COMMAND ${install_command}
        RESULT_VARIABLE install_status
        OUTPUT_VARIABLE install_output
        ERROR_VARIABLE install_error
    )
    if(NOT install_status EQUAL 0)
        message(FATAL_ERROR
            "Staged portal ${component} install failed:\n${install_output}${install_error}")
    endif()
endforeach()

set(portal_executable
    "${install_prefix}/${QINDAQT_INSTALL_LIBEXECDIR}/xdg-desktop-portal-qindaqt")
file(GLOB policy_libraries LIST_DIRECTORIES false
    "${install_prefix}/${QINDAQT_INSTALL_LIBDIR}/*qindaqt_portal_appearance*")
file(GLOB service_libraries LIST_DIRECTORIES false
    "${install_prefix}/${QINDAQT_INSTALL_LIBDIR}/*qindaqt_portal_service*")
list(LENGTH policy_libraries policy_library_count)
list(LENGTH service_libraries service_library_count)
if(NOT policy_library_count EQUAL 1 OR NOT service_library_count EQUAL 1)
    message(FATAL_ERROR "Staged portal package must contain its two exact libraries")
endif()
set(dbus_descriptor
    "${install_prefix}/${QINDAQT_INSTALL_DBUSSERVICEDIR}/org.freedesktop.impl.portal.desktop.qindaqt.service")
set(systemd_unit
    "${install_prefix}/${QINDAQT_INSTALL_SYSTEMDUSERUNITDIR}/xdg-desktop-portal-qindaqt.service")
set(portal_metadata
    "${install_prefix}/${QINDAQT_INSTALL_DATADIR}/xdg-desktop-portal/portals/qindaqt.portal")
set(capture_portal_metadata
    "${install_prefix}/${QINDAQT_INSTALL_DATADIR}/xdg-desktop-portal/portals/qindaqt.capture.portal")
set(portal_selection
    "${install_prefix}/${QINDAQT_INSTALL_DATADIR}/xdg-desktop-portal/qindaqt-portals.conf")
set(kde_portal_dropin
    "${install_prefix}/${QINDAQT_INSTALL_SYSTEMDUSERUNITDIR}/plasma-xdg-desktop-portal-kde.service.d/20-qindaqt-remotedesktop.conf")
set(theme_directory
    "${install_prefix}/${QINDAQT_INSTALL_DATADIR}/qindaqt/themes")
set(consent_executable "${install_prefix}/${QINDAQT_INSTALL_LIBEXECDIR}/qindaqt-portal-consent")
set(chooser_executable "${install_prefix}/${QINDAQT_INSTALL_LIBEXECDIR}/qindaqt-portal-chooser")
set(capture_executable "${install_prefix}/${QINDAQT_INSTALL_LIBEXECDIR}/qindaqt-portal-capture")
set(capture_desktop "${install_prefix}/${QINDAQT_INSTALL_DATADIR}/applications/org.qindaqt.PortalCapture.desktop")
set(capture_broker "${install_prefix}/${QINDAQT_INSTALL_LIBEXECDIR}/qindaqt-portal-capture-backend")
set(capture_broker_desktop "${install_prefix}/${QINDAQT_INSTALL_DATADIR}/applications/org.qindaqt.PortalCaptureBackend.desktop")
set(uri_relay "${install_prefix}/${QINDAQT_INSTALL_LIBEXECDIR}/qindaqt-uri-relay")
foreach(required_artifact IN ITEMS portal_executable consent_executable chooser_executable uri_relay dbus_descriptor systemd_unit
        portal_metadata portal_selection)
    if(NOT EXISTS "${${required_artifact}}")
        message(FATAL_ERROR "Staged portal package misses ${required_artifact}: ${${required_artifact}}")
    endif()
endforeach()

# Protected capture is optional only when the selected fork public contract is
# unavailable. Its absent product cannot advertise/activate a capture family.
if(QINDAQT_PROTECTED_CAPTURE_AVAILABLE)
    set(capture_metadata_check_args "-DPORTAL_CAPTURE_METADATA_FILE=${capture_portal_metadata}")
    foreach(artifact capture_executable capture_desktop capture_broker capture_broker_desktop capture_portal_metadata)
        if(NOT EXISTS "${${artifact}}")
            message(FATAL_ERROR "Qualified protected capture package misses ${artifact}")
        endif()
    endforeach()
    if(NOT EXISTS "${QINDAQT_CAPTURE_AUTHORITY_PATHS}")
        message(FATAL_ERROR "Qualified protected capture package lost selected public launch paths")
    endif()
    file(READ "${QINDAQT_CAPTURE_AUTHORITY_PATHS}" selected_launch_paths)
    set(expected_BrokerExecutable "${QINDAQT_EXPECTED_CAPTURE_LIBEXEC}/qindaqt-portal-capture-backend")
    set(expected_HelperExecutable "${QINDAQT_EXPECTED_CAPTURE_LIBEXEC}/qindaqt-portal-capture")
    set(expected_BrokerDesktop "${QINDAQT_EXPECTED_CAPTURE_DATA}/applications/org.qindaqt.PortalCaptureBackend.desktop")
    set(expected_HelperDesktop "${QINDAQT_EXPECTED_CAPTURE_DATA}/applications/org.qindaqt.PortalCapture.desktop")
    foreach(name BrokerExecutable HelperExecutable BrokerDesktop HelperDesktop)
        string(REGEX MATCH "${name}\\[\\] = \"([^\"]+)\"" constant "${selected_launch_paths}")
        if(NOT constant OR NOT "${CMAKE_MATCH_1}" STREQUAL "${expected_${name}}")
            message(FATAL_ERROR "Staged protected capture/selected fork fixed path mismatch: ${name}")
        endif()
    endforeach()
    file(STRINGS "${capture_desktop}" helper_exec REGEX "^Exec=")
    file(STRINGS "${capture_broker_desktop}" broker_exec REGEX "^Exec=")
    if(NOT helper_exec STREQUAL "Exec=${expected_HelperExecutable}"
       OR NOT broker_exec STREQUAL "Exec=${expected_BrokerExecutable}")
        message(FATAL_ERROR "Staged protected capture desktop Exec differs from fixed selected fork image")
    endif()
    file(READ "${capture_desktop}" capture_permission_entry)
    file(READ "${capture_broker_desktop}" capture_broker_entry)
    if(NOT capture_permission_entry MATCHES "X-QindaQt-KWin-DBus-Restricted-Interfaces=org.qindaqt.KWin.ScreenShot2"
       OR capture_broker_entry MATCHES "X-(QindaQt|KDE).*Interfaces=")
        message(FATAL_ERROR "Capture helper compatibility entry/broker ambient permission contract differs")
    endif()
else()
    set(capture_metadata_check_args)
    foreach(artifact capture_executable capture_desktop capture_broker capture_broker_desktop capture_portal_metadata)
        if(EXISTS "${${artifact}}")
            message(FATAL_ERROR "Unavailable protected capture left an unqualified artifact: ${artifact}")
        endif()
    endforeach()
endif()
if(EXISTS "${install_prefix}/${QINDAQT_INSTALL_DATADIR}/applications/org.qindaqt.PortalBackend.desktop"
   OR EXISTS "${install_prefix}/${QINDAQT_INSTALL_DBUSSERVICEDIR}/org.freedesktop.impl.portal.desktop.qindaqt.capture.service")
    message(FATAL_ERROR "Capture-only authority gained ambient general-backend permission or activation")
endif()

# AGENT-GUARD: These names are integration entry points discovered by external
# daemons. A duplicate anywhere in the staged component makes package selection
# order-dependent even if the canonical path itself is correct.
set(portal_singletons
        "org.freedesktop.impl.portal.desktop.qindaqt.service"
        "xdg-desktop-portal-qindaqt.service"
        "qindaqt.portal"
        "qindaqt-portals.conf")
if(QINDAQT_PROTECTED_CAPTURE_AVAILABLE)
    list(APPEND portal_singletons "qindaqt.capture.portal")
endif()
foreach(singleton IN LISTS portal_singletons)
    file(GLOB_RECURSE matches LIST_DIRECTORIES false
         "${install_prefix}/*/${singleton}" "${install_prefix}/${singleton}")
    list(LENGTH matches match_count)
    if(NOT match_count EQUAL 1)
        message(FATAL_ERROR
            "Staged portal package has ${match_count} copies of ${singleton}")
    endif()
endforeach()
if(NOT EXISTS "${theme_directory}/qinda-dark.json")
    message(FATAL_ERROR "Staged portal runtime misses its QST theme catalog")
endif()

file(READ "${dbus_descriptor}" dbus_content)
file(READ "${systemd_unit}" unit_content)
file(READ "${portal_metadata}" portal_content)
file(READ "${portal_selection}" selection_content)
if(QINDAQT_PROTECTED_CAPTURE_AVAILABLE)
    file(READ "${capture_portal_metadata}" capture_portal_content)
endif()
foreach(content IN ITEMS dbus_content unit_content portal_content capture_portal_content selection_content)
    if("${${content}}" MATCHES "@[A-Za-z0-9_]+@|${QINDAQT_BUILD_DIRECTORY}|${SOURCE_PORTAL_ROOT}")
        message(FATAL_ERROR "Staged portal metadata contains a template or build/source path")
    endif()
endforeach()
if(EXISTS "${kde_portal_dropin}")
    message(FATAL_ERROR "Native portal package still installs a KDE backend override")
endif()
if(NOT dbus_content MATCHES "Name=org.freedesktop.impl.portal.desktop.qindaqt"
   OR NOT dbus_content MATCHES "SystemdService=xdg-desktop-portal-qindaqt.service"
   OR NOT dbus_content MATCHES "Exec=${QINDAQT_EXPECTED_PORTAL_EXECUTABLE}")
    message(FATAL_ERROR "Staged D-Bus activation descriptor is not exact")
endif()
if(NOT unit_content MATCHES "Type=dbus"
   OR NOT unit_content MATCHES "BusName=org.freedesktop.impl.portal.desktop.qindaqt"
   OR NOT unit_content MATCHES "ExecStart=${QINDAQT_EXPECTED_PORTAL_EXECUTABLE}"
   OR NOT unit_content MATCHES "RestrictAddressFamilies=AF_UNIX"
   OR NOT unit_content MATCHES "NoNewPrivileges=true")
    message(FATAL_ERROR "Staged portal systemd residency/hardening contract is incomplete")
endif()
execute_process(
    COMMAND "${QINDAQT_CMAKE}"
            "-DPORTAL_ROOT=${SOURCE_PORTAL_ROOT}"
            "-DSTAGE_ROOT=${install_prefix}/${QINDAQT_INSTALL_INCLUDEDIR}"
            "-DPORTAL_METADATA_FILE=${portal_metadata}"
            ${capture_metadata_check_args}
            "-DPORTAL_SELECTION_FILE=${portal_selection}"
            -P "${CHECK_SCRIPT}"
    RESULT_VARIABLE boundary_status
    OUTPUT_VARIABLE boundary_output
    ERROR_VARIABLE boundary_error
)
if(NOT boundary_status EQUAL 0)
    message(FATAL_ERROR
        "Staged portal boundary failed:\n${boundary_output}${boundary_error}")
endif()

if(DEFINED QINDAQT_FRONTEND_TEST)
    foreach(required IN ITEMS QINDAQT_TOOLKIT_PROBE QINDAQT_XDG_DESKTOP_PORTAL
            QINDAQT_FALLBACK_PORTAL QINDAQT_DBUS_RUN_SESSION)
        if(NOT DEFINED ${required})
            message(FATAL_ERROR "Missing staged Portal P1 input: ${required}")
        endif()
    endforeach()
    function(run_staged_portal_proof test_binary mode label)
        execute_process(
            COMMAND "${QINDAQT_CMAKE}" -E env
                --unset=DBUS_SESSION_BUS_ADDRESS
                --unset=DBUS_STARTER_ADDRESS
                --unset=DBUS_STARTER_BUS_TYPE
                "DBUS_SYSTEM_BUS_ADDRESS=unix:path=${install_prefix}/unavailable-system-bus"
                "QINDAQT_TEST_PORTAL_EXECUTABLE=${portal_executable}"
                "QINDAQT_TEST_PORTAL_METADATA=${portal_metadata}"
                "QINDAQT_TEST_PORTAL_SELECTION=${portal_selection}"
                "QINDAQT_TEST_PORTAL_THEME_DIR=${theme_directory}"
                "QINDAQT_TEST_SETTINGS_EXECUTABLE=${QINDAQT_EXPECTED_SETTINGS_EXECUTABLE}"
                "QINDAQT_TEST_SETTINGS_SCHEMA_DIR=${QINDAQT_EXPECTED_SCHEMA_DIR}"
                "QINDAQT_TEST_TOOLKIT_PROBE=${QINDAQT_TOOLKIT_PROBE}"
                "${QINDAQT_DBUS_RUN_SESSION}" --
                "${test_binary}" "${mode}"
            RESULT_VARIABLE proof_status
            OUTPUT_VARIABLE proof_output
            ERROR_VARIABLE proof_error
        )
        if(NOT proof_status EQUAL 0)
            message(FATAL_ERROR
                "Staged Portal ${label} ${mode} proof failed:\n${proof_output}${proof_error}")
        endif()
    endfunction()
    foreach(mode IN ITEMS selection toolkit)
        run_staged_portal_proof("${QINDAQT_FRONTEND_TEST}" "${mode}" "P1")
    endforeach()
    if(DEFINED QINDAQT_ROUTING_TEST AND NOT QINDAQT_ROUTING_TEST STREQUAL "")
        foreach(mode IN ITEMS routing routing-negative)
            run_staged_portal_proof("${QINDAQT_ROUTING_TEST}" "${mode}" "routing")
        endforeach()
    endif()
endif()

execute_process(
    COMMAND "${QINDAQT_CMAKE}" -E env
        "QINDAQT_TEST_PORTAL_EXECUTABLE=${portal_executable}"
        "QINDAQT_TEST_SETTINGS_EXECUTABLE=${QINDAQT_EXPECTED_SETTINGS_EXECUTABLE}"
        "QINDAQT_TEST_SETTINGS_SCHEMA_DIR=${QINDAQT_EXPECTED_SCHEMA_DIR}"
        "QINDAQT_TEST_PORTAL_THEME_DIR=${theme_directory}"
        "${QINDAQT_PROCESS_TEST}"
    RESULT_VARIABLE lifecycle_status
    OUTPUT_VARIABLE lifecycle_output
    ERROR_VARIABLE lifecycle_error
)
if(NOT lifecycle_status EQUAL 0)
    message(FATAL_ERROR
        "Staged portal process lifecycle failed:\n${lifecycle_output}${lifecycle_error}")
endif()

function(expect_installed_metadata_rejection label expected)
    execute_process(
        COMMAND "${QINDAQT_CMAKE}"
                "-DPORTAL_ROOT=${SOURCE_PORTAL_ROOT}"
                "-DSTAGE_ROOT=${install_prefix}/${QINDAQT_INSTALL_INCLUDEDIR}"
                "-DPORTAL_METADATA_FILE=${portal_metadata}"
                ${capture_metadata_check_args}
                "-DPORTAL_SELECTION_FILE=${portal_selection}"
                -P "${CHECK_SCRIPT}"
        RESULT_VARIABLE status
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
    )
    if(status EQUAL 0)
        message(FATAL_ERROR
            "Installed portal checker accepted ${label} metadata poison")
    endif()
    string(CONCAT combined "${output}" "${error}")
    if(NOT combined MATCHES "${expected}")
        message(FATAL_ERROR
            "Installed ${label} poison was rejected for the wrong reason:\n${combined}")
    endif()
endfunction()

# Mutation-sensitive installed controls use a standard interface present in
# the pinned 1.20.4 dependency. Each artifact must independently reject it.
file(WRITE "${portal_metadata}"
    "[portal]\nDBusName=org.freedesktop.impl.portal.desktop.qindaqt\nInterfaces=org.freedesktop.impl.portal.Settings;org.freedesktop.impl.portal.Background\nUseIn=QindaQt\n")
expect_installed_metadata_rejection(
    ".portal" "exact native resident interfaces")
file(WRITE "${portal_metadata}" "${portal_content}")

file(WRITE "${portal_selection}"
    "[preferred]\ndefault=*\norg.freedesktop.impl.portal.Settings=qindaqt\norg.freedesktop.impl.portal.Background=qindaqt\n")
expect_installed_metadata_rejection(
    "selector" "exact native routing policy")
file(WRITE "${portal_selection}" "${selection_content}")

file(WRITE "${portal_selection}"
    "[preferred]\ndefault=none\norg.freedesktop.impl.portal.Settings=qindaqt\norg.freedesktop.impl.portal.Access=kde;gtk;lxqt\norg.freedesktop.impl.portal.AppChooser=kde;gtk;lxqt\norg.freedesktop.impl.portal.FileChooser=kde;gtk;lxqt\norg.freedesktop.impl.portal.Email=kde;gtk;lxqt\norg.freedesktop.impl.portal.Inhibit=kde;gtk;lxqt\norg.freedesktop.impl.portal.Notification=kde;gtk;lxqt\norg.freedesktop.impl.portal.Print=kde;gtk;lxqt\norg.freedesktop.impl.portal.Screenshot=kde;gtk;lxqt\norg.freedesktop.impl.portal.ScreenCast=kde;gtk;lxqt\norg.freedesktop.impl.portal.RemoteDesktop=kde;gtk;lxqt\norg.freedesktop.impl.portal.GlobalShortcuts=kde\norg.freedesktop.impl.portal.InputCapture=kde\norg.freedesktop.impl.portal.Clipboard=kde\norg.freedesktop.impl.portal.Usb=kde\norg.freedesktop.impl.portal.Account=kde\norg.freedesktop.impl.portal.DynamicLauncher=kde\norg.freedesktop.impl.portal.Wallpaper=none\norg.freedesktop.impl.portal.Background=none\n")
expect_installed_metadata_rejection(
    "selector-secret-drop" "exact native routing policy")
file(WRITE "${portal_selection}" "${selection_content}")

# Repeat actual frontend positive and closed-default withdrawal controls with
# installed metadata/URI relay. Input remains the explicit production-source
# test driver; this does not claim physical installed consent qualification.
execute_process(
    COMMAND "${QINDAQT_CMAKE}" -E env --unset=DBUS_SESSION_BUS_ADDRESS
        python3 "${QINDAQT_NATIVE_FRONTEND_RUNNER}" "${QINDAQT_NATIVE_FRONTEND_TEST}"
        "${QINDAQT_NATIVE_CONSENT_INPUT}" "${QINDAQT_NATIVE_COMPOSITOR}"
        "${uri_relay}" "${QINDAQT_NATIVE_MAIL}" "${portal_metadata}" "${portal_selection}"
    RESULT_VARIABLE native_frontend_status OUTPUT_VARIABLE native_frontend_output ERROR_VARIABLE native_frontend_error)
if(NOT native_frontend_status EQUAL 0)
    message(FATAL_ERROR "Staged native frontend qualification failed:\n${native_frontend_output}${native_frontend_error}")
endif()
message(STATUS "Staged native frontend and routing withdrawal controls pass")

# Self-guard: a private header planted in the disposable installed namespace
# must make the same checker fail.
file(WRITE
    "${install_prefix}/${QINDAQT_INSTALL_INCLUDEDIR}/qindaqt/services/portal/private_adapter.h"
    "#include <QDBusArgument>\n")
execute_process(
    COMMAND "${QINDAQT_CMAKE}"
            "-DPORTAL_ROOT=${SOURCE_PORTAL_ROOT}"
            "-DSTAGE_ROOT=${install_prefix}/${QINDAQT_INSTALL_INCLUDEDIR}"
            "-DPORTAL_METADATA_FILE=${portal_metadata}"
            ${capture_metadata_check_args}
            "-DPORTAL_SELECTION_FILE=${portal_selection}"
            -P "${CHECK_SCRIPT}"
    RESULT_VARIABLE poison_status
    OUTPUT_VARIABLE poison_output
    ERROR_VARIABLE poison_error
)
if(poison_status EQUAL 0)
    message(FATAL_ERROR "Installed portal checker accepted a private-header poison")
endif()

file(REMOVE_RECURSE "${install_prefix}")
message(STATUS "Staged portal package, private lifecycle, and installed poison passed")

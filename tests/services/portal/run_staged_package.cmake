# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required IN ITEMS QINDAQT_CMAKE QINDAQT_BUILD_DIRECTORY
        QINDAQT_INSTALL_PREFIX QINDAQT_INSTALL_INCLUDEDIR QINDAQT_INSTALL_LIBDIR
        QINDAQT_INSTALL_LIBEXECDIR QINDAQT_INSTALL_DBUSSERVICEDIR
        QINDAQT_INSTALL_SYSTEMDUSERUNITDIR QINDAQT_INSTALL_DATADIR
        QINDAQT_EXPECTED_PORTAL_EXECUTABLE QINDAQT_EXPECTED_SETTINGS_EXECUTABLE
        QINDAQT_EXPECTED_SCHEMA_DIR QINDAQT_PROCESS_TEST CHECK_SCRIPT
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
set(portal_selection
    "${install_prefix}/${QINDAQT_INSTALL_DATADIR}/xdg-desktop-portal/qindaqt-portals.conf")
set(kde_portal_dropin
    "${install_prefix}/${QINDAQT_INSTALL_SYSTEMDUSERUNITDIR}/plasma-xdg-desktop-portal-kde.service.d/20-qindaqt-remotedesktop.conf")
set(theme_directory
    "${install_prefix}/${QINDAQT_INSTALL_DATADIR}/qindaqt/themes")
foreach(required_artifact IN ITEMS portal_executable dbus_descriptor systemd_unit
        portal_metadata portal_selection kde_portal_dropin)
    if(NOT EXISTS "${${required_artifact}}")
        message(FATAL_ERROR "Staged portal package misses ${required_artifact}: ${${required_artifact}}")
    endif()
endforeach()

# AGENT-GUARD: These names are integration entry points discovered by external
# daemons. A duplicate anywhere in the staged component makes package selection
# order-dependent even if the canonical path itself is correct.
foreach(singleton IN ITEMS
        "org.freedesktop.impl.portal.desktop.qindaqt.service"
        "xdg-desktop-portal-qindaqt.service"
        "qindaqt.portal"
        "qindaqt-portals.conf")
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
file(READ "${kde_portal_dropin}" kde_portal_dropin_content)
foreach(content IN ITEMS dbus_content unit_content portal_content selection_content)
    if("${${content}}" MATCHES "@[A-Za-z0-9_]+@|${QINDAQT_BUILD_DIRECTORY}|${SOURCE_PORTAL_ROOT}")
        message(FATAL_ERROR "Staged portal metadata contains a template or build/source path")
    endif()
endforeach()
if(NOT kde_portal_dropin_content MATCHES "\\[Service\\]"
   OR NOT kde_portal_dropin_content MATCHES "Environment=XDG_CURRENT_DESKTOP=KDE"
   OR kde_portal_dropin_content MATCHES "XDG_CURRENT_DESKTOP=QindaQt")
    message(FATAL_ERROR "Staged KDE portal compatibility drop-in is not exact")
endif()
if(DEFINED QINDAQT_KDE_PORTAL_DBUS_SERVICE
   AND NOT QINDAQT_KDE_PORTAL_DBUS_SERVICE STREQUAL ""
   AND EXISTS "${QINDAQT_KDE_PORTAL_DBUS_SERVICE}")
    file(READ "${QINDAQT_KDE_PORTAL_DBUS_SERVICE}" kde_dbus_content)
    if(NOT kde_dbus_content MATCHES
           "SystemdService=plasma-xdg-desktop-portal-kde\\.service")
        message(FATAL_ERROR
            "Host KDE D-Bus activation does not target the drop-in-covered systemd unit")
    endif()
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
    foreach(mode IN ITEMS selection toolkit)
        execute_process(
            COMMAND "${QINDAQT_CMAKE}" -E env
                --unset=DBUS_SESSION_BUS_ADDRESS
                --unset=DBUS_STARTER_ADDRESS
                --unset=DBUS_STARTER_BUS_TYPE
                "QINDAQT_TEST_PORTAL_EXECUTABLE=${portal_executable}"
                "QINDAQT_TEST_PORTAL_METADATA=${portal_metadata}"
                "QINDAQT_TEST_PORTAL_SELECTION=${portal_selection}"
                "QINDAQT_TEST_PORTAL_THEME_DIR=${theme_directory}"
                "QINDAQT_TEST_SETTINGS_EXECUTABLE=${QINDAQT_EXPECTED_SETTINGS_EXECUTABLE}"
                "QINDAQT_TEST_SETTINGS_SCHEMA_DIR=${QINDAQT_EXPECTED_SCHEMA_DIR}"
                "QINDAQT_TEST_TOOLKIT_PROBE=${QINDAQT_TOOLKIT_PROBE}"
                "${QINDAQT_DBUS_RUN_SESSION}" --
                "${QINDAQT_FRONTEND_TEST}" "${mode}"
            RESULT_VARIABLE frontend_status
            OUTPUT_VARIABLE frontend_output
            ERROR_VARIABLE frontend_error
        )
        if(NOT frontend_status EQUAL 0)
            message(FATAL_ERROR
                "Staged Portal P1 ${mode} proof failed:\n${frontend_output}${frontend_error}")
        endif()
    endforeach()
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
    ".portal" "exact singleton Settings interface")
file(WRITE "${portal_metadata}" "${portal_content}")

file(WRITE "${portal_selection}"
    "[preferred]\ndefault=*\norg.freedesktop.impl.portal.Settings=qindaqt\norg.freedesktop.impl.portal.Background=qindaqt\n")
expect_installed_metadata_rejection(
    "selector" "exact Settings/fallback routing policy")
file(WRITE "${portal_selection}" "${selection_content}")

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

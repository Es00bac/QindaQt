# SPDX-License-Identifier: GPL-3.0-or-later

if(NOT DEFINED PORTAL_ROOT)
    message(FATAL_ERROR "PORTAL_ROOT is required")
endif()

file(GLOB_RECURSE portal_sources LIST_DIRECTORIES false
     "${PORTAL_ROOT}/*.h" "${PORTAL_ROOT}/*.cpp" "${PORTAL_ROOT}/CMakeLists.txt")
foreach(source IN LISTS portal_sources)
    file(READ "${source}" content)
    if(content MATCHES "services/(settings_service|network|display)|apps/settings|shell_customization|src/session")
        message(FATAL_ERROR "Portal crosses a prohibited product boundary in ${source}")
    endif()
    if(content MATCHES "QtQml|QtQuick|QProcess|NetworkManager|Wayland|KWin")
        message(FATAL_ERROR "Portal gained presentation/platform/process authority in ${source}")
    endif()
    string(REGEX MATCHALL
        "org\\.freedesktop\\.impl\\.portal\\.[A-Z][A-Za-z0-9]*"
        backend_interfaces "${content}")
    foreach(backend_interface IN LISTS backend_interfaces)
        if(NOT backend_interface STREQUAL
           "org.freedesktop.impl.portal.Settings")
            message(FATAL_ERROR
                "Portal imports an out-of-scope standard interface in ${source}: ${backend_interface}")
        endif()
    endforeach()
endforeach()

foreach(policy_file IN ITEMS
        "${PORTAL_ROOT}/include/qindaqt/services/portal/appearance_policy.h"
        "${PORTAL_ROOT}/include/qindaqt/services/portal/appearance_theme_catalog.h"
        "${PORTAL_ROOT}/src/appearance_policy.cpp"
        "${PORTAL_ROOT}/src/appearance_theme_catalog.cpp")
    if(EXISTS "${policy_file}")
        file(READ "${policy_file}" content)
        if(content MATCHES "QDBus|SettingsClient|QtSettingsTransport")
            message(FATAL_ERROR "Appearance policy imports transport in ${policy_file}")
        endif()
    endif()
endforeach()

if(DEFINED PORTAL_METADATA_FILE)
    set(portal_file "${PORTAL_METADATA_FILE}")
else()
    set(portal_file "${PORTAL_ROOT}/data/qindaqt.portal")
endif()
if(NOT EXISTS "${portal_file}")
    message(FATAL_ERROR "Portal metadata file is missing: ${portal_file}")
endif()
file(READ "${portal_file}" portal_content)
set(expected_portal_content
    "[portal]\nDBusName=org.freedesktop.impl.portal.desktop.qindaqt\nInterfaces=org.freedesktop.impl.portal.Settings\nUseIn=QindaQt\n")
if(NOT portal_content STREQUAL expected_portal_content)
    message(FATAL_ERROR
        "Portal .portal differs from exact singleton Settings interface")
endif()

if(DEFINED PORTAL_SELECTION_FILE)
    set(selection_file "${PORTAL_SELECTION_FILE}")
else()
    set(selection_file "${PORTAL_ROOT}/data/qindaqt-portals.conf")
endif()
if(NOT EXISTS "${selection_file}")
    message(FATAL_ERROR "Portal selection config is missing: ${selection_file}")
endif()
file(READ "${selection_file}" selection_content)
set(expected_selection_content
    "[preferred]\ndefault=none\norg.freedesktop.impl.portal.Settings=qindaqt\norg.freedesktop.impl.portal.Access=kde;gtk;lxqt\norg.freedesktop.impl.portal.AppChooser=kde;gtk;lxqt\norg.freedesktop.impl.portal.FileChooser=kde;gtk;lxqt\norg.freedesktop.impl.portal.Email=kde;gtk;lxqt\norg.freedesktop.impl.portal.Inhibit=kde;gtk;lxqt\norg.freedesktop.impl.portal.Notification=kde;gtk;lxqt\norg.freedesktop.impl.portal.Print=kde;gtk;lxqt\norg.freedesktop.impl.portal.Screenshot=kde;gtk;lxqt\norg.freedesktop.impl.portal.ScreenCast=kde;gtk;lxqt\norg.freedesktop.impl.portal.RemoteDesktop=kde;gtk;lxqt\norg.freedesktop.impl.portal.GlobalShortcuts=kde\norg.freedesktop.impl.portal.Secret=gnome-keyring\norg.freedesktop.impl.portal.InputCapture=kde\norg.freedesktop.impl.portal.Clipboard=kde\norg.freedesktop.impl.portal.Usb=kde\norg.freedesktop.impl.portal.Account=kde\norg.freedesktop.impl.portal.DynamicLauncher=kde\norg.freedesktop.impl.portal.Wallpaper=none\norg.freedesktop.impl.portal.Background=none\n")
if(NOT selection_content STREQUAL expected_selection_content)
    message(FATAL_ERROR
        "Portal selector differs from exact Settings/fallback routing policy")
endif()

if(DEFINED STAGE_ROOT)
    file(GLOB_RECURSE installed_portal_headers LIST_DIRECTORIES false
         "${STAGE_ROOT}/qindaqt/services/portal/*.h")
    list(LENGTH installed_portal_headers installed_header_count)
    if(NOT installed_header_count EQUAL 5)
        message(FATAL_ERROR
            "Installed portal boundary has ${installed_header_count} headers instead of 5")
    endif()
    foreach(header IN LISTS installed_portal_headers)
        get_filename_component(name "${header}" NAME)
        if(name MATCHES "(_p|private|adapter)\\.h$")
            message(FATAL_ERROR "Installed portal boundary leaked a private header: ${header}")
        endif()
        file(READ "${header}" content)
        if(content MATCHES "QDBusArgument|portal_settings_object_p|QtSettingsTransport")
            message(FATAL_ERROR "Installed portal header leaked a private transport type: ${header}")
        endif()
    endforeach()
endif()

message(STATUS "Portal keeps appearance policy, Settings1 source, D-Bus transport, and packaging separated")

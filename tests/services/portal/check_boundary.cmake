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
    if(content MATCHES "org\\.freedesktop\\.impl\\.portal\\.(FileChooser|OpenURI|Notification|Inhibit|ScreenCast|RemoteDesktop)")
        message(FATAL_ERROR "Portal advertises an out-of-scope portal interface in ${source}")
    endif()
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

set(portal_file "${PORTAL_ROOT}/data/qindaqt.portal")
if(EXISTS "${portal_file}")
    file(READ "${portal_file}" content)
    if(NOT content MATCHES "DBusName=org.freedesktop.impl.portal.desktop.qindaqt"
       OR NOT content MATCHES "Interfaces=org.freedesktop.impl.portal.Settings"
       OR content MATCHES "Interfaces=[^\n]*(FileChooser|OpenURI|Notification|Inhibit|ScreenCast|RemoteDesktop)")
        message(FATAL_ERROR "Portal metadata does not expose the exact Settings-only boundary")
    endif()
endif()

set(selection_file "${PORTAL_ROOT}/data/qindaqt-portals.conf")
if(EXISTS "${selection_file}")
    file(READ "${selection_file}" content)
    if(NOT content MATCHES "org.freedesktop.impl.portal.Settings=qindaqt"
       OR content MATCHES "org.freedesktop.impl.portal.(FileChooser|OpenURI|Notification|Inhibit|ScreenCast|RemoteDesktop)=qindaqt")
        message(FATAL_ERROR "Portal selection config is not Settings-only")
    endif()
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

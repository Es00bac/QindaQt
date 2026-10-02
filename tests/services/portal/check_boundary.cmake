# SPDX-License-Identifier: GPL-3.0-or-later
cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED PORTAL_ROOT)
    message(FATAL_ERROR "PORTAL_ROOT is required")
endif()

file(GLOB_RECURSE portal_sources LIST_DIRECTORIES false
     "${PORTAL_ROOT}/*.h" "${PORTAL_ROOT}/*.cpp" "${PORTAL_ROOT}/CMakeLists.txt")
# Appearance keeps its original pure boundary. Explicit independent family
# files below are the ADR0318 process/platform composition boundary, not an
# exemption for unknown future files or arbitrary standard interfaces.
set(foundation_files
    # ADR-0334: native shortcuts session adapter and ordinary consent helper.
    "shortcuts/CMakeLists.txt"
    "shortcuts/global_shortcuts_adaptor.h"
    "shortcuts/global_shortcuts_adaptor.cpp"
    "shortcuts/shortcut_sessions.cpp"
    "shortcuts/shortcut_sessions_p.h"
    "shortcuts/shortcut_wire.h"
    "shortcuts/shortcut_wire.cpp"
    "shortcuts/shortcut_ui.h"
    "shortcuts/process_shortcuts.cpp"
    "shortcuts/helper_main.cpp"
    "misc_families/CMakeLists.txt"
    "misc_families/account_adaptor.cpp"
    "misc_families/account_adaptor.h"
    "misc_families/account_policy.cpp"
    "misc_families/launcher_adaptor.cpp"
    "misc_families/launcher_adaptor.h"
    "misc_families/launcher_policy.cpp"
    "misc_families/main.cpp"
    "misc_families/misc_dialog.cpp"
    "misc_families/misc_dialog.h"
    "misc_families/misc_policy.cpp"
    "misc_families/misc_policy.h"
    "misc_families/misc_ui.h"
    "misc_families/print_adaptor.cpp"
    "misc_families/print_adaptor.h"
    "misc_families/print_arguments.cpp"
    "misc_families/print_conversion.h"
    "misc_families/print_job.cpp"
    "misc_families/print_job.h"
    "misc_families/print_page_sizes.cpp"
    "misc_families/print_policy.cpp"
    "misc_families/print_policy.h"
    "misc_families/print_settings_load.cpp"
    "misc_families/print_settings_save.cpp"
    "misc_families/process_misc.cpp"
    "misc_families/process_misc.h"
    "misc_families/usb_adaptor.cpp"
    "misc_families/usb_adaptor.h"
    "misc_families/usb_policy.cpp"
    "app/main.cpp"
    "CMakeLists.txt"
    "foundation/CMakeLists.txt"
    "consent/CMakeLists.txt"
    "consent/main.cpp"
    "consent/consent_controller.h"
    "consent/consent_controller.cpp"
    "choosers/CMakeLists.txt"
    "choosers/main.cpp"
    "choosers/chooser_dialog.h"
    "choosers/chooser_dialog.cpp"
    "choosers/choice_controls.h"
    "choosers/choice_controls.cpp"
    "choosers/file_filter_proxy.h"
    "choosers/file_filter_proxy.cpp"
    "choosers/file_chooser_dialog.h"
    "choosers/file_chooser_dialog.cpp"
    "choosers/app_chooser_dialog.h"
    "choosers/app_chooser_dialog.cpp"
    "src/chooser_wire.cpp"
    "src/chooser_frames.cpp"
    "src/file_chooser_policy.cpp"
    "src/file_chooser_adaptor.cpp"
    "src/app_chooser_policy.cpp"
    "src/app_chooser_adaptor.cpp"
    "src/process_chooser.cpp"
    "include/qindaqt/services/portal/chooser_types.h"
    "include/qindaqt/services/portal/chooser_ui.h"
    "include/qindaqt/services/portal/file_chooser_adaptor.h"
    "include/qindaqt/services/portal/app_chooser_adaptor.h"
    "include/qindaqt/services/portal/process_chooser.h"
    "capture/CMakeLists.txt"
    "capture/main.cpp"
    "capture/helper_runtime.h"
    "capture/helper_runtime.cpp"
    "capture/authority/packet.h"
    "capture/authority/packet.cpp"
    "capture/authority/channel.h"
    "capture/authority/channel.cpp"
    "capture/backend/authority_capture.h"
    "capture/backend/authority_capture.cpp"
    "capture/backend/main.cpp"
    "capture/native_capture_admission.h"
    "capture/native_capture_admission.cpp"
    "capture/capture_dialog.h"
    "capture/capture_dialog.cpp"
    "capture/color_picker.h"
    "capture/color_picker.cpp"
    "src/capture_policy.cpp"
    "src/capture_wire.cpp"
    "src/capture_sessions.cpp"
    "src/capture_sessions_p.h"
    "src/screenshot_adaptor.cpp"
    "src/screencast_adaptor.cpp"
    "src/process_capture.cpp"
    "include/qindaqt/services/portal/capture_types.h"
    "include/qindaqt/services/portal/capture_ui.h"
    "include/qindaqt/services/portal/process_capture.h"
    "include/qindaqt/services/portal/screenshot_adaptor.h"
    "include/qindaqt/services/portal/screencast_adaptor.h"
    "src/access_adaptor.cpp"
    "src/access_policy.cpp"
    "src/email_adaptor.cpp"
    "src/email_policy.cpp"
    "src/foundation_composition.cpp"
    "src/idle_inhibition.cpp"
    "src/inhibit_adaptor.cpp"
    "src/native_notifications.cpp"
    "src/notification_adaptor.cpp"
    "src/notification_icon.cpp"
    "src/notification_policy.cpp"
    "src/process_consent.cpp"
    "src/request_registry.cpp"
    "src/session_binding.cpp"
    "include/qindaqt/services/portal/access_adaptor.h"
    "include/qindaqt/services/portal/access_consent.h"
    "include/qindaqt/services/portal/email_adaptor.h"
    "include/qindaqt/services/portal/email_policy.h"
    "include/qindaqt/services/portal/foundation_composition.h"
    "include/qindaqt/services/portal/idle_inhibition.h"
    "include/qindaqt/services/portal/inhibit_adaptor.h"
    "include/qindaqt/services/portal/native_notifications.h"
    "include/qindaqt/services/portal/notification_adaptor.h"
    "include/qindaqt/services/portal/notification_policy.h"
    "include/qindaqt/services/portal/process_consent.h"
    "include/qindaqt/services/portal/request_registry.h"
    "include/qindaqt/services/portal/session_binding.h"
    # ADR-0335 remote input: compositor EIS D-Bus client and adaptors only.
    "remote_input/CMakeLists.txt"
    "remote_input/src/compositor_eis.cpp"
    "remote_input/src/remote_sessions.cpp"
    "remote_input/src/remote_sessions_p.h"
    "remote_input/src/remote_desktop_adaptor.cpp"
    "remote_input/src/input_capture_adaptor.cpp"
    "remote_input/src/clipboard_adaptor_p.h"
    "remote_input/src/clipboard_adaptor.cpp"
    "remote_input/include/qindaqt/services/portal/remote_input/compositor_eis.h"
    "remote_input/include/qindaqt/services/portal/remote_input/remote_desktop_adaptor.h"
    "remote_input/include/qindaqt/services/portal/remote_input/input_capture_adaptor.h"
)
foreach(source IN LISTS portal_sources)
    file(READ "${source}" content)
    file(RELATIVE_PATH relative "${PORTAL_ROOT}" "${source}")
    if(content MATCHES "services/(settings_service|network|display)|shell_customization|src/session/")
        message(FATAL_ERROR "Portal crosses a prohibited product boundary in ${source}")
    endif()
    if(NOT relative IN_LIST foundation_files)
        if(content MATCHES "apps/settings")
            message(FATAL_ERROR "Portal crosses a prohibited product boundary in ${source}")
        endif()
        if(content MATCHES "QtQml|QtQuick|QProcess|NetworkManager|Wayland|KWin")
            message(FATAL_ERROR "Portal gained presentation/platform/process authority in ${source}")
        endif()
    endif()
    string(REGEX MATCHALL "org\\.freedesktop\\.impl\\.portal\\.[A-Z][A-Za-z0-9]*" backend_interfaces "${content}")
    foreach(backend_interface IN LISTS backend_interfaces)
        if(NOT backend_interface STREQUAL "org.freedesktop.impl.portal.Settings")
            if(NOT relative IN_LIST foundation_files OR NOT backend_interface MATCHES
"^org\\.freedesktop\\.impl\\.portal\\.(Access|Notification|Email|Inhibit|Request|FileChooser|AppChooser|Screenshot|ScreenCast|Session|Print|Account|DynamicLauncher|Usb|RemoteDesktop|InputCapture|Clipboard|GlobalShortcuts)$")
                message(FATAL_ERROR "Portal imports an out-of-scope standard interface in ${source}: ${backend_interface}")
            endif()
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
    "[portal]\nDBusName=org.freedesktop.impl.portal.desktop.qindaqt\nInterfaces=org.freedesktop.impl.portal.Settings;org.freedesktop.impl.portal.Secret;org.freedesktop.impl.portal.Access;org.freedesktop.impl.portal.Notification;org.freedesktop.impl.portal.Email;org.freedesktop.impl.portal.FileChooser;org.freedesktop.impl.portal.AppChooser\nUseIn=QindaQt\n")
if(NOT portal_content STREQUAL expected_portal_content)
    message(FATAL_ERROR
        "Portal .portal differs from exact Settings and Secret interfaces")
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
    "[preferred]\ndefault=none\norg.freedesktop.impl.portal.Settings=qindaqt\norg.freedesktop.impl.portal.Access=qindaqt\norg.freedesktop.impl.portal.AppChooser=qindaqt\norg.freedesktop.impl.portal.FileChooser=qindaqt\norg.freedesktop.impl.portal.Email=qindaqt\norg.freedesktop.impl.portal.Inhibit=kde;gtk;lxqt\norg.freedesktop.impl.portal.Notification=qindaqt\norg.freedesktop.impl.portal.Print=kde;gtk;lxqt\norg.freedesktop.impl.portal.Screenshot=kde;gtk;lxqt\norg.freedesktop.impl.portal.ScreenCast=kde;gtk;lxqt\norg.freedesktop.impl.portal.RemoteDesktop=kde;gtk;lxqt\norg.freedesktop.impl.portal.GlobalShortcuts=kde\norg.freedesktop.impl.portal.Secret=qindaqt\norg.freedesktop.impl.portal.InputCapture=kde\norg.freedesktop.impl.portal.Clipboard=kde\norg.freedesktop.impl.portal.Usb=kde\norg.freedesktop.impl.portal.Account=kde\norg.freedesktop.impl.portal.DynamicLauncher=kde\norg.freedesktop.impl.portal.Wallpaper=none\norg.freedesktop.impl.portal.Background=none\n")
if(NOT selection_content STREQUAL expected_selection_content)
    message(FATAL_ERROR
        "Portal selector differs from exact Settings/fallback routing policy")
endif()

if(DEFINED STAGE_ROOT)
    file(GLOB_RECURSE installed_portal_headers LIST_DIRECTORIES false
         "${STAGE_ROOT}/qindaqt/services/portal/*.h")
    set(expected_headers
        "compositor_eis.h"
        "input_capture_adaptor.h"
        "remote_desktop_adaptor.h"
        "misc_ui.h"
        "misc_policy.h"
        "print_policy.h"
        "process_misc.h"
        "account_adaptor.h"
        "usb_adaptor.h"
        "launcher_adaptor.h"
        "print_adaptor.h"
        "access_adaptor.h"
        "access_consent.h"
        "appearance_policy.h"
        "appearance_source.h"
        "appearance_theme_catalog.h"
        "app_chooser_adaptor.h"
        "chooser_types.h"
        "chooser_ui.h"
        "capture_types.h"
        "capture_ui.h"
        "process_capture.h"
        "screenshot_adaptor.h"
        "screencast_adaptor.h"
        "email_adaptor.h"
        "email_policy.h"
        "file_chooser_adaptor.h"
        "foundation_composition.h"
        "idle_inhibition.h"
        "inhibit_adaptor.h"
        "native_notifications.h"
        "notification_adaptor.h"
        "notification_policy.h"
        "process_consent.h"
        "process_chooser.h"
        "request_registry.h"
        "resident_portal_service.h"
        "session_binding.h"
        "settings1_appearance_source.h"
)
    set(actual_headers)
    foreach(header IN LISTS installed_portal_headers)
        get_filename_component(name "${header}" NAME)
        list(APPEND actual_headers "${name}")
    endforeach()
    list(SORT actual_headers)
    list(SORT expected_headers)
    if(NOT actual_headers STREQUAL expected_headers)
        message(FATAL_ERROR "Installed portal boundary differs from exact public family headers: ${actual_headers}")
    endif()

    foreach(header IN LISTS installed_portal_headers)
        get_filename_component(name "${header}" NAME)
        if(name MATCHES "(_p|private|adapter)\\.h$")
            message(FATAL_ERROR "Installed portal boundary leaked a private header: ${header}")
        endif()
        file(READ "${header}" content)
        if(content MATCHES "portal_settings_object_p|QtSettingsTransport")
            message(FATAL_ERROR "Installed portal header leaked a private transport type: ${header}")
        endif()
        if(name MATCHES "^(appearance_|resident_portal_service|settings1_appearance_source)" AND content MATCHES "QDBusArgument")
            message(FATAL_ERROR "Appearance header leaked a private transport type: ${header}")
        endif()
    endforeach()
endif()

message(STATUS "Portal keeps appearance policy, Settings1 source, D-Bus transport, and packaging separated")

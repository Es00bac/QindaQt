# SPDX-License-Identifier: GPL-3.0-or-later

if(NOT DEFINED QINDAQT_SOURCE_DIR)
    message(FATAL_ERROR "QINDAQT_SOURCE_DIR is required")
endif()

if(NOT DEFINED QINDAQT_BREEZE_ICON_FIXTURE)
    set(QINDAQT_BREEZE_ICON_FIXTURE
        "${QINDAQT_SOURCE_DIR}/tests/shell/testdata/breeze-icon-names.txt")
endif()
file(READ "${QINDAQT_BREEZE_ICON_FIXTURE}"
     breeze_icon_names)
string(REPLACE "\r\n" "\n" breeze_icon_names "${breeze_icon_names}")

function(require_breeze_icon icon_name)
    string(FIND "\n${breeze_icon_names}\n" "\n${icon_name}\n" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "Declared icon '${icon_name}' is not resolvable in the pinned Breeze fixture")
    endif()
endfunction()

function(require_declaration relative_path declaration)
    file(READ "${QINDAQT_SOURCE_DIR}/${relative_path}" source)
    string(FIND "${source}" "${declaration}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "${relative_path} lacks required icon-first declaration: ${declaration}")
    endif()
endfunction()

function(require_icon relative_path icon_name)
    require_declaration("${relative_path}" "\"${icon_name}\"")
    require_breeze_icon("${icon_name}")
endfunction()

# AGENT-CONTRACT: This is the complete production panel inventory. Adding a
# BuiltinAppletRegistry entry without adding its presentation declaration here
# is a test failure instead of a silently text-only panel regression.
require_declaration("src/shell/launcher/qml/LauncherApplet.qml" "launcherAppletIcon")
require_declaration("src/shell/audio_applet/qml/AudioApplet.qml" "audioAppletIcon")
require_declaration("src/shell/bluetooth_applet/qml/BluetoothApplet.qml" "bluetoothAppletIcon")
require_declaration("src/shell/power_applet/qml/PowerApplet.qml" "powerAppletIcon")
require_declaration("src/shell/clipboard_applet/qml/ClipboardPanelApplet.qml" "clipboardPanelIcon")
require_declaration("src/shell/task_list/applet/qml/TaskListEntryButton.qml" "taskListEntryIcon")
require_declaration("src/shell/status_notifier/applet/qml/StatusNotifierItemDelegate.qml" "statusNotifierItemIcon")
require_declaration("src/shell/qml/NotificationCenterApplet.qml" "notificationCenterAppletGlyph")
require_declaration("src/shell/qml/ClockApplet.qml" "clockApplet")
require_declaration("src/shell/global_menu/applet/qml/GlobalMenuApplet.qml" "globalMenuTopLevelItem")
# Desktop controls (docs/wiki/shell/desktop-controls.md): one icon-first
# declaration per registered entry point.
require_declaration("src/shell/desktop_controls/qml/WorkspaceSwitcherApplet.qml" "workspaceSwitcherStrip")
require_declaration("src/shell/desktop_controls/qml/WorkspaceTilesApplet.qml" "workspaceTilesStrip")
require_declaration("src/shell/desktop_controls/qml/WorkspaceStrip.qml" "workspaceStripPlaceholder")
require_declaration("src/shell/desktop_controls/qml/ShowDesktopApplet.qml" "showDesktopButton")
require_declaration("src/shell/desktop_controls/qml/OverviewTriggerApplet.qml" "overviewTriggerButton")
require_declaration("src/shell/desktop_controls/qml/ActiveApplicationApplet.qml" "activeApplicationIcon")
require_declaration("src/shell/desktop_controls/qml/SystemMenuApplet.qml" "systemMenuButton")
require_declaration("src/shell/desktop_controls/qml/SystemStatusApplet.qml" "systemStatusLaneIcon")
require_declaration("src/shell/desktop_controls/qml/PlacesMenuApplet.qml" "placesMenuButton")
require_declaration("src/shell/desktop_controls/qml/QuickLaunchApplet.qml" "quickLaunchEntryIcon")
require_declaration("src/shell/desktop_controls/qml/CommandPaletteApplet.qml" "commandPaletteButton")
require_declaration("src/shell/desktop_controls/qml/CommandHudApplet.qml" "commandHudButton")
require_declaration("src/shell/desktop_controls/qml/DashboardApplet.qml" "dashboardButton")
require_declaration("src/shell/desktop_controls/qml/DesktopControlButton.qml" "desktopControlIcon")

# AGENT-GUARD: this is the complete literal name set selected by built-in
# summary policy. The fixture is the intersection of the Breeze and
# Breeze-dark chains used by shipped themes; declaration alone is not enough.
foreach(icon_name IN ITEMS
        application-x-executable applications-other audio-volume-muted
        battery-missing clock edit-paste network-bluetooth-inactive-symbolic
        network-wireless-disconnected network-wireless-off
        network-wireless-signal-excellent network-wireless-signal-good
        network-wireless-signal-ok network-wireless-signal-weak notifications
        preferences-plugin preferences-system-windows start-here-kde
        system-search user-desktop view-grid virtual-desktops)
    require_icon("src/shell/qml/AppletChip.qml" "${icon_name}")
endforeach()

foreach(icon_name IN ITEMS audio-volume-muted audio-volume-low
                           audio-volume-medium audio-volume-high)
    require_icon("src/shell/audio_applet/qml/AudioApplet.qml" "${icon_name}")
endforeach()
foreach(icon_name IN ITEMS battery-missing battery-000 battery-020 battery-040
                           battery-060 battery-080 battery-100)
    require_icon("src/shell/power_applet/qml/PowerApplet.qml" "${icon_name}")
endforeach()
foreach(icon_name IN ITEMS network-bluetooth-activated
                           network-bluetooth-inactive-symbolic)
    require_icon("src/shell/bluetooth_applet/qml/BluetoothApplet.qml" "${icon_name}")
endforeach()
require_icon("src/shell/launcher/qml/LauncherApplet.qml" "start-here-kde")
require_icon("src/shell/clipboard_applet/qml/ClipboardPanelApplet.qml" "edit-paste")
require_icon("src/shell/status_notifier/applet/qml/StatusNotifierApplet.qml"
             "view-more-symbolic")
foreach(icon_name IN ITEMS view-refresh-symbolic preferences-system-windows
                           dialog-warning)
    require_icon("src/shell/task_list/applet/qml/TaskListApplet.qml" "${icon_name}")
endforeach()

# Desktop controls: literal summary and menu icon names.
require_icon("src/shell/desktop_controls/qml/WorkspaceStrip.qml" "virtual-desktops")
require_icon("src/shell/desktop_controls/qml/ShowDesktopApplet.qml" "user-desktop")
require_icon("src/shell/desktop_controls/qml/OverviewTriggerApplet.qml" "view-grid")
require_icon("src/shell/desktop_controls/qml/CommandPaletteApplet.qml" "system-search")
require_icon("src/shell/desktop_controls/qml/CommandHudApplet.qml" "edit-find")
require_icon("src/shell/desktop_controls/qml/ActiveApplicationApplet.qml" "preferences-system-windows")
foreach(icon_name IN ITEMS window-minimize window-close)
    require_icon("src/shell/desktop_controls/qml/ActiveApplicationApplet.qml" "${icon_name}")
endforeach()
foreach(icon_name IN ITEMS preferences-system help-about system-lock-screen system-log-out
                           system-suspend system-reboot system-shutdown)
    require_icon("src/shell/desktop_controls/qml/SystemMenuApplet.qml" "${icon_name}")
endforeach()
require_icon("src/shell/desktop_controls/qml/SystemStatusApplet.qml" "preferences-plugin")
foreach(icon_name IN ITEMS audio-volume-muted audio-volume-low audio-volume-medium
                           audio-volume-high network-bluetooth-activated
                           network-bluetooth-inactive-symbolic battery-missing
                           battery-000 battery-020 battery-040 battery-060
                           battery-080 battery-100)
    require_icon("src/shell/desktop_controls/src/system_status_controller.cpp" "${icon_name}")
endforeach()
require_icon("src/shell/desktop_controls/qml/PlacesMenuApplet.qml" "folder")
foreach(icon_name IN ITEMS user-home user-desktop folder-documents folder-download
                           folder-music folder-pictures folder-videos drive-harddisk)
    require_icon("src/shell/desktop_controls/src/places_controller.cpp" "${icon_name}")
endforeach()
require_icon("src/shell/desktop_controls/qml/QuickLaunchApplet.qml" "applications-other")
require_icon("src/shell/desktop_controls/qml/DashboardApplet.qml" "dashboard-show")
foreach(icon_name IN ITEMS virtual-desktops user-desktop)
    require_icon("src/shell/desktop_controls/src/command_search_controller.cpp" "${icon_name}")
endforeach()

foreach(theme IN ITEMS qinda-dark qinda-dusk qinda-high-contrast qinda-light qinda-macos)
    require_declaration("data/themes/${theme}.json" "\"iconTheme\"")
endforeach()

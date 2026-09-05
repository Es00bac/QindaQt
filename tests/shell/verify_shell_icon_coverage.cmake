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

foreach(theme IN ITEMS qinda-dark qinda-dusk qinda-high-contrast qinda-light qinda-macos)
    require_declaration("data/themes/${theme}.json" "\"iconTheme\"")
endforeach()

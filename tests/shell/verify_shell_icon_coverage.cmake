# SPDX-License-Identifier: GPL-3.0-or-later

if(NOT DEFINED QINDAQT_SOURCE_DIR)
    message(FATAL_ERROR "QINDAQT_SOURCE_DIR is required")
endif()

function(require_declaration relative_path declaration)
    file(READ "${QINDAQT_SOURCE_DIR}/${relative_path}" source)
    string(FIND "${source}" "${declaration}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "${relative_path} lacks required icon-first declaration: ${declaration}")
    endif()
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
require_declaration("src/shell/qml/AppletChip.qml" "network-wireless-disconnected")
require_declaration("src/shell/qml/AppletChip.qml" "grouped-task-list")

foreach(theme IN ITEMS qinda-dark qinda-dusk qinda-high-contrast qinda-light qinda-macos)
    require_declaration("data/themes/${theme}.json" "\"iconTheme\"")
endforeach()

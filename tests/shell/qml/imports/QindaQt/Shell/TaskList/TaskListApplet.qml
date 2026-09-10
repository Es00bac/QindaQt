// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

// Dispatcher-only double. The compiled production QindaQt.Shell.TaskList
// applet has its own C++ offscreen interaction test; this source module keeps
// the dispatcher mapping test independent of static-plugin registration in
// qmltestrunner. It mirrors the production boundary exactly: `access` and
// `vertical` only — presentation tokens come from QindaQt.Tokens, never from
// an injected theme map.
Item {
    required property var access
    property bool vertical: false
    property bool dockMode: false
    property int dockTileSize: 60
    property bool dockHasLauncherGroup: false
    property bool reducedMotion: false
    property bool dockZoomEnabled: true

    objectName: "taskListApplet"
    implicitWidth: 96
    implicitHeight: 32
    enabled: access !== null
    Accessible.role: Accessible.Grouping
    Accessible.name: "Task list"
}

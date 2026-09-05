// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

// Dispatcher-only double for the compiled QindaQt.Shell.DesktopControls
// module. The production component is the real pinned-launcher tile grid;
// this stub preserves only the public access/vertical construction boundary.
Item {
    required property var access
    property bool vertical: false

    objectName: "applicationTilesApplet"
    implicitWidth: 96
    implicitHeight: 56
    enabled: access !== null
}

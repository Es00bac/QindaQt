// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QindaQt.Shell.Icons 1.0 as ShellIcons
import QindaQt.Tokens 1.0

// Dispatcher-only double. The compiled production module has a separate
// fatal-warning keyboard/accessibility test; this fixture verifies only the
// shared panel inventory and purpose-specific facade propagation.
Item {
    id: root

    required property var access
    property bool vertical: false
    readonly property int summaryIconExtent:
        Math.max(0, Math.min(20, height - Tokens.space["2"]))

    objectName: "launcherApplet"
    implicitWidth: 64
    implicitHeight: 28
    enabled: access !== null

    ShellIcons.Icon {
        objectName: "launcherAppletIcon"
        anchors.centerIn: parent
        name: "start-here-kde"
        size: root.summaryIconExtent
        color: Tokens.fg.default
        fallbackText: "Applications"
    }
}

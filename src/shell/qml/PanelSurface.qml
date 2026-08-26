// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QindaQt.Shell

Item {
    id: root

    // AGENT-NOTE: This wrapper intentionally approximates geometry only for the
    // standalone concept preview. Runtime windows consume shell_layout output
    // in C++; PanelContent is the shared presentation boundary.
    required property var panel
    required property var theme
    required property real desktopWidth
    required property real desktopHeight
    readonly property bool horizontal: panel.edge === "top" || panel.edge === "bottom"
    readonly property real scaledThickness: Math.max(22, panel.thickness * Math.min(desktopWidth / 1920, 1.25))

    width: horizontal ? desktopWidth * panel.length : scaledThickness
    height: horizontal ? scaledThickness * panel.rows : desktopHeight * panel.length
    x: panel.edge === "left" ? 0
       : panel.edge === "right" ? desktopWidth - width
       : panel.alignment === "start" ? 0
       : panel.alignment === "end" ? desktopWidth - width
       : (desktopWidth - width) / 2
    y: panel.edge === "top" ? 0
       : panel.edge === "bottom" ? desktopHeight - height
       : panel.alignment === "start" ? 0
       : panel.alignment === "end" ? desktopHeight - height
       : (desktopHeight - height) / 2
    opacity: panel.layer === "below" ? 0.82 : 0.96
    z: panel.layer === "below" ? 0 : panel.layer === "overlay" ? 15 : 10

    PanelContent {
        anchors.fill: parent
        panel: root.panel
        theme: root.theme
    }
}

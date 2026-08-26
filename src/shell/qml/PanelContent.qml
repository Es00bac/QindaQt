// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

Item {
    id: root

    required property var panel
    required property var theme
    property bool liveApplets: false
    readonly property bool horizontal: panel.edge === "top" || panel.edge === "bottom"
    readonly property var colors: theme.colors ?? ({})

    clip: true

    Rectangle {
        anchors.fill: parent
        radius: root.panel.alignment === "fill" ? 0 : root.theme.cornerRadius ?? 10
        color: root.colors.surface ?? "#222624"
        border.color: root.colors.border ?? "#3c433f"
        border.width: 1
    }

    PanelAppletRow {
        anchors.left: parent.left
        anchors.leftMargin: 4
        anchors.top: parent.top
        anchors.topMargin: 4
        height: parent.height - 8
        visible: root.horizontal
        zone: "start"
        panel: root.panel
        theme: root.theme
        liveApplets: root.liveApplets
    }

    PanelAppletRow {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 4
        height: parent.height - 8
        visible: root.horizontal
        zone: "center"
        panel: root.panel
        theme: root.theme
        liveApplets: root.liveApplets
    }

    PanelAppletRow {
        anchors.right: parent.right
        anchors.rightMargin: 4
        anchors.top: parent.top
        anchors.topMargin: 4
        height: parent.height - 8
        visible: root.horizontal
        zone: "end"
        panel: root.panel
        theme: root.theme
        liveApplets: root.liveApplets
    }

    PanelAppletColumn {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 4
        visible: !root.horizontal
        zone: "start"
        panel: root.panel
        theme: root.theme
        liveApplets: root.liveApplets
    }

    PanelAppletColumn {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.margins: 4
        visible: !root.horizontal
        zone: "center"
        panel: root.panel
        theme: root.theme
        liveApplets: root.liveApplets
    }

    PanelAppletColumn {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 4
        visible: !root.horizontal
        zone: "end"
        panel: root.panel
        theme: root.theme
        liveApplets: root.liveApplets
    }
}

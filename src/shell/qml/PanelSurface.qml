// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QindaQt.Shell

Rectangle {
    id: root

    required property var panel
    required property var theme
    required property real desktopWidth
    required property real desktopHeight
    readonly property bool horizontal: panel.edge === "top" || panel.edge === "bottom"
    readonly property real scaledThickness: Math.max(22, panel.thickness * Math.min(desktopWidth / 1920, 1.25))
    readonly property var colors: theme.colors ?? ({})

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
    radius: panel.length < 0.98 ? theme.cornerRadius ?? 10 : 0
    color: colors.surface ?? "#222624"
    border.color: colors.border ?? "#3c433f"
    border.width: 1
    opacity: panel.layer === "below" ? 0.82 : 0.96
    z: panel.layer === "below" ? 0 : panel.layer === "overlay" ? 15 : 10
    clip: true

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
    }

    Column {
        id: verticalApplets

        anchors.fill: parent
        anchors.margins: 4
        spacing: 4
        visible: !root.horizontal

        Repeater {
            model: root.panel.applets ?? []

            AppletChip {
                required property var modelData
                applet: modelData
                theme: root.theme
                vertical: true
                width: verticalApplets.width
            }
        }
    }
}

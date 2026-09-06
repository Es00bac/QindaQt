// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QindaQt.Tokens 1.0

T.Control {
    id: root

    required property var customizeSettings
    padding: Tokens.space["4"]
    Accessible.ignored: true

    background: Rectangle {
        color: Tokens.bg.base
        radius: Tokens.radius.l
        border.width: Tokens.space["1"] / 2
        border.color: Tokens.outline.divider
    }

    contentItem: Item {
        id: canvasHost

        Rectangle {
            id: outputFrame
            objectName: "customizeOutputCanvas"
            anchors.centerIn: parent
            width: Math.min(parent.width, parent.height * 16 / 9)
            height: width * 9 / 16
            color: Tokens.bg.base
            border.width: Tokens.space["1"] / 2
            border.color: Tokens.outline.strong
            radius: Tokens.radius.s

            Text {
                anchors.centerIn: parent
                text: qsTr("Desktop preview · 1920 × 1080")
                color: Tokens.fg.muted
                font.family: Tokens.type.fontFamily
                font.pointSize: Tokens.type.caption
            }

            Repeater {
                model: root.customizeSettings.panels

                delegate: Rectangle {
                    id: panelSurface
                    required property var modelData

                    readonly property real canvasScale: outputFrame.width / 1920
                    readonly property bool horizontal:
                        panelSurface.modelData.edge === "top"
                        || panelSurface.modelData.edge === "bottom"

                    objectName: "customizeCanvasPanel_" + panelSurface.modelData.id
                    x: panelSurface.modelData.x * canvasScale
                    y: panelSurface.modelData.y * canvasScale
                    width: panelSurface.modelData.width * canvasScale
                    height: panelSurface.modelData.height * canvasScale
                    color: Tokens.bg.highest
                    border.width: root.customizeSettings.visualDragActive
                                  ? Tokens.space["1"] : Tokens.space["1"] / 2
                    border.color: root.customizeSettings.dropAccepted
                                  ? Tokens.status.success.foreground
                                  : Tokens.accent.default
                    radius: Tokens.radius.s

                    TapHandler {
                        onTapped: root.customizeSettings.selectPanel(
                                      panelSurface.modelData.id)
                    }

                    Repeater {
                        model: ["start", "center", "end"]

                        delegate: Item {
                            id: zone
                            required property string modelData
                            required property int index

                            readonly property var zoneApplets:
                                panelSurface.modelData.applets.filter(
                                    item => item.zone === zone.modelData)

                            x: panelSurface.horizontal
                               ? index * panelSurface.width / 3 : 0
                            y: panelSurface.horizontal
                               ? 0 : index * panelSurface.height / 3
                            width: panelSurface.horizontal
                                   ? panelSurface.width / 3 : panelSurface.width
                            height: panelSurface.horizontal
                                    ? panelSurface.height : panelSurface.height / 3

                            Rectangle {
                                anchors.fill: parent
                                anchors.margins: Tokens.space["1"] / 2
                                radius: Tokens.radius.s
                                color: root.customizeSettings.visualDragActive
                                       ? Tokens.state.hover : "transparent"
                                border.width: root.customizeSettings.visualDragActive
                                              ? Tokens.space["1"] / 2 : 0
                                border.color: Tokens.outline.strong
                            }

                            readonly property string targetPanelId: panelSurface.modelData.id
                            readonly property string targetZone: modelData
                            objectName: "customizeDrop_" + targetPanelId + "_" + targetZone

                            Flow {
                                anchors.fill: parent
                                anchors.margins: Tokens.space["1"]
                                spacing: Tokens.space["1"]
                                clip: true

                                Repeater {
                                    model: zone.zoneApplets

                                    delegate: Rectangle {
                                        id: chip
                                        required property var modelData

                                        // A representative panel is scaled from a real
                                        // 1920px output. Full applet labels do not fit in
                                        // its thin strips and used to leak over the preview.
                                        // Markers preserve panel flow while the palette and
                                        // outline carry the readable names.
                                        width: Math.max(4, Math.min(12, zone.width - 2))
                                        height: Math.max(3, Math.min(8, zone.height - 2))
                                        color: Tokens.accent.subtle
                                        border.width: Tokens.space["1"] / 2
                                        border.color: Tokens.outline.strong
                                        radius: Math.min(width, height) / 2
                                        Accessible.role: Accessible.ListItem
                                        Accessible.name: qsTr("%1 applet").arg(chip.modelData.name)
                                        Accessible.description: qsTr("Shown in the %1 zone")
                                                                    .arg(zone.modelData)

                                        TapHandler {
                                            onTapped: root.customizeSettings.selectApplet(
                                                panelSurface.modelData.id,
                                                chip.modelData.id)
                                        }
                                        readonly property string dragPluginId: modelData.pluginId
                                        readonly property string dragPanelId: panelSurface.modelData.id
                                        readonly property string dragAppletId: modelData.id
                                        objectName: "customizeChip_" + dragAppletId

                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

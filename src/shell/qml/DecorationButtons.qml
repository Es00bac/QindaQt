// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

Item {
    id: root

    required property var theme
    readonly property var decoration: theme.decoration ?? ({})
    readonly property var colors: theme.colors ?? ({})
    readonly property bool trafficLights: decoration.buttonStyle === "traffic-lights"
    readonly property bool glyphsVisible: !trafficLights
                                          || !decoration.hoverGlyphs
                                          || clusterHover.hovered

    implicitWidth: trafficLights ? 58 : 74
    implicitHeight: 30

    HoverHandler {
        id: clusterHover
    }

    Row {
        anchors.centerIn: parent
        spacing: root.trafficLights ? 7 : 10

        Repeater {
            model: root.trafficLights
                   ? [
                         { "glyph": "x", "color": root.decoration.closeColor },
                         { "glyph": "_", "color": root.decoration.minimizeColor },
                         { "glyph": "[]", "color": root.decoration.maximizeColor }
                     ]
                   : [
                         { "glyph": "—", "color": "transparent" },
                         { "glyph": "□", "color": "transparent" },
                         { "glyph": "×", "color": "transparent" }
                     ]

            Rectangle {
                required property var modelData

                width: root.trafficLights ? 13 : 14
                height: width
                radius: root.trafficLights ? width / 2 : 2
                color: modelData.color
                border.width: root.trafficLights ? 1 : 0
                border.color: Qt.darker(modelData.color, 1.12)

                Text {
                    anchors.centerIn: parent
                    anchors.verticalCenterOffset: root.trafficLights && modelData.glyph === "_" ? -2 : 0
                    visible: root.glyphsVisible
                    text: modelData.glyph
                    color: root.trafficLights ? "#3A302D" : root.colors.textMuted ?? "#a9afa9"
                    font.pixelSize: root.trafficLights ? 8 : 13
                    font.bold: root.trafficLights
                }
            }
        }
    }
}

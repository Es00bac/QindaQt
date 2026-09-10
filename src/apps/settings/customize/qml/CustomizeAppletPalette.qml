// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

// Applet palette: an icon grid (a horizontal strip in compact layouts).
// Tiles carry the plugin glyph; names and descriptions live on tooltips and
// accessible names, keeping the palette visual instead of a text list.
T.Control {
    id: root

    required property var customizeSettings
    property bool compact: false
    readonly property Item firstFocusTarget: root.compact
                                                  ? stripView.count > 0
                                                    ? stripView.itemAtIndex(0)
                                                    : null
                                                  : gridView.count > 0
                                                    ? gridView.itemAtIndex(0)
                                                    : null

    padding: Tokens.space["3"]
    Accessible.role: Accessible.Grouping
    Accessible.name: qsTr("Applet palette")

    background: Rectangle {
        color: Tokens.bg.raised
        radius: Tokens.radius.m
        border.width: Tokens.space["1"] / 2
        border.color: Tokens.outline.divider
    }

    contentItem: ColumnLayout {
        spacing: Tokens.space["2"]

        Label {
            Layout.fillWidth: true
            text: root.compact ? qsTr("Add applets")
                               : qsTr("Drag an applet onto the desktop preview")
            muted: true
            font.pointSize: Tokens.type.caption
            wrapMode: Text.Wrap
            visible: !root.compact
            Accessible.name: text
        }

        // Wide: two columns of icon tiles.
        GridView {
            id: gridView

            objectName: "customizePalette"
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: !root.compact
            clip: true
            cellWidth: width / 2
            cellHeight: 92
            model: root.customizeSettings.palette
            Accessible.role: Accessible.List
            Accessible.name: qsTr("Available applets")

            delegate: CustomizePaletteTile {
                customizeSettings: root.customizeSettings
                tileSize: 84
                glyphSize: 28
            }

            T.ScrollBar.vertical: T.ScrollBar {
                policy: T.ScrollBar.AsNeeded
                activeFocusOnTab: false
                Accessible.name: qsTr("Available applets scroll position")
            }
        }

        // Compact: a single-row strip of smaller tiles.
        ListView {
            id: stripView

            objectName: "customizePaletteStrip"
            Layout.fillWidth: true
            Layout.preferredHeight: 76
            visible: root.compact
            orientation: ListView.Horizontal
            spacing: Tokens.space["2"]
            clip: true
            model: root.customizeSettings.palette
            Accessible.role: Accessible.List
            Accessible.name: qsTr("Available applets")

            delegate: CustomizePaletteTile {
                customizeSettings: root.customizeSettings
                tileSize: 64
                glyphSize: 22
            }

            T.ScrollBar.horizontal: T.ScrollBar {
                policy: T.ScrollBar.AsNeeded
                activeFocusOnTab: false
                Accessible.name: qsTr("Available applets scroll position")
            }
        }
    }
}

// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

T.Control {
    id: root

    required property var customizeSettings
    property bool compact: false
    readonly property Item firstFocusTarget: paletteView.count > 0
                                                  ? paletteView.itemAtIndex(0)
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

        SectionHeader {
            Layout.fillWidth: true
            title: qsTr("Applets")
            description: qsTr("Drag an applet to a highlighted panel zone, or focus it and press Enter")
        }

        ListView {
            id: paletteView
            objectName: "customizePalette"
            Layout.fillWidth: true
            Layout.fillHeight: true
            implicitHeight: root.compact ? 64 : 220
            orientation: root.compact ? ListView.Horizontal : ListView.Vertical
            spacing: Tokens.space["2"]
            clip: true
            model: root.customizeSettings.palette
            Accessible.role: Accessible.List
            Accessible.name: qsTr("Available applets")

            delegate: Button {
                id: paletteButton
                required property var modelData

                objectName: "customizePalette_" + paletteButton.modelData.id
                width: root.compact ? Math.max(140, implicitWidth) : paletteView.width
                text: paletteButton.modelData.name
                available: root.customizeSettings.canEdit
                emphasized: false
                accessibleDescription: paletteButton.modelData.description
                Accessible.role: Accessible.ListItem
                Accessible.name: qsTr("%1 applet").arg(text)
                onClicked: {
                    const panels = root.customizeSettings.panels
                    if (panels.length > 0) {
                        root.customizeSettings.keyboardInsert(
                            paletteButton.modelData.id, panels[0].id, "start", "")
                    }
                }

                DragHandler {
                    id: paletteDrag
                    target: null
                    enabled: paletteButton.available
                    onActiveChanged: {
                        if (active) {
                            root.customizeSettings.startPaletteDrag(
                                paletteButton.modelData.id)
                        } else if (root.customizeSettings.visualDragActive) {
                            root.customizeSettings.cancelDrag()
                        }
                    }
                }

                Drag.active: paletteDrag.active
                Drag.source: paletteButton
                Drag.mimeData: ({
                    "application/x-qindaqt-customize-applet":
                        paletteButton.modelData.id
                })
            }
        }
    }
}

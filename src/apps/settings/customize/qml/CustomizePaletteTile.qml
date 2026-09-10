// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

// One palette tile: the applet's icon, recognized by glyph and explained by
// tooltip. Activation inserts into the first panel's start zone through the
// same public intent a pointer drag uses.
T.AbstractButton {
    id: tile

    required property var modelData
    required property var customizeSettings
    property int tileSize: 84
    property int glyphSize: 28

    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    enabled: customizeSettings.canEdit
    implicitWidth: tileSize
    implicitHeight: tileSize
    padding: 0

    objectName: "customizePalette_" + (modelData.id ?? "")
    Accessible.role: Accessible.ListItem
    Accessible.name: qsTr("%1 applet").arg(modelData.name ?? "")
    Accessible.description: modelData.description ?? ""
    T.ToolTip.visible: hovered
    T.ToolTip.delay: 500
    T.ToolTip.text: (modelData.name ?? "") + "\n" + (modelData.description ?? "")

    readonly property string dragPluginId: modelData.id ?? ""
    readonly property string dragPanelId: ""
    readonly property string dragAppletId: ""

    onClicked: {
        const panels = customizeSettings.panels
        if (panels.length > 0) {
            customizeSettings.keyboardInsert(
                tile.modelData.id ?? "", panels[0].id, "start", "")
        }
    }

    background: Rectangle {
        radius: Tokens.radius.m
        color: !tile.enabled ? Tokens.bg.base
             : tile.pressed ? Tokens.state.pressed
             : tile.hovered ? Tokens.state.hover : Tokens.bg.raised
        border.width: tile.activeFocus ? Tokens.space["1"] : Tokens.space["1"] / 2
        border.color: tile.activeFocus ? Tokens.focus.ring
                     : Tokens.outline.divider

        FocusRing {
            anchors.fill: parent
            control: tile
        }
    }

    contentItem: Item {
        CustomizeAppletIcon {
            anchors.centerIn: parent
            pluginId: tile.modelData.id ?? ""
            iconSize: tile.glyphSize
        }
    }
}

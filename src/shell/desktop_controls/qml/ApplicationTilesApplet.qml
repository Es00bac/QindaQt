// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0 as C
import QindaQt.Shell.Icons 1.0 as ShellIcons
import QindaQt.Tokens 1.0

// NeXTSTEP-style application dock. The launcher facade supplies the same
// bounded, pinned catalog used by Quick Launch; each tile follows the one
// audited activation path, so a visible tile is a real launch intent.
Item {
    id: root

    required property var access
    property bool vertical: false

    readonly property bool ready: access !== null && Tokens.ready
    readonly property var rows: ready ? access.rows : []
    readonly property bool showRows: ready && rows.length > 0
    readonly property int tileExtent: vertical ? Math.max(48, width - Tokens.space["2"])
                                               : 64

    objectName: "applicationTilesApplet"
    implicitWidth: showRows ? strip.implicitWidth : placeholder.implicitWidth + Tokens.space["2"]
    implicitHeight: showRows ? strip.implicitHeight : 28

    Accessible.role: Accessible.Grouping
    Accessible.name: qsTr("Applications")
    Accessible.description: !ready ? qsTr("Application tiles are unavailable")
                            : rows.length === 0 ? qsTr("No pinned applications")
                            : qsTr("%1 launchable applications").arg(rows.length)

    ShellIcons.Icon {
        id: placeholder
        objectName: "applicationTilesPlaceholder"
        visible: !root.showRows
        anchors.centerIn: parent
        name: "applications-other"
        size: 18
        color: Tokens.fg.disabled
        symbolic: true
        fallbackText: qsTr("Applications")
        Accessible.ignored: true
    }

    GridLayout {
        id: strip
        anchors.fill: parent
        visible: root.showRows
        flow: root.vertical ? GridLayout.TopToBottom : GridLayout.LeftToRight
        rows: root.vertical ? -1 : 1
        columns: root.vertical ? 1 : -1
        rowSpacing: Tokens.space["1"]
        columnSpacing: Tokens.space["1"]

        Repeater {
            model: root.rows

            T.Button {
                id: tile
                required property var modelData
                required property int index

                objectName: "applicationTile"
                Layout.preferredWidth: root.vertical ? root.tileExtent : 64
                Layout.preferredHeight: root.vertical ? 64 : root.tileExtent
                padding: Tokens.space["1"]
                focusPolicy: Qt.TabFocus
                hoverEnabled: true
                enabled: root.ready && Boolean(root.access.available)

                Accessible.role: Accessible.Button
                Accessible.name: String(modelData.accessibleName)
                Accessible.description: String(modelData.accessibleDescription)

                function activate() { root.access.activate(String(modelData.entryId)) }
                onClicked: activate()
                Keys.onReturnPressed: activate()
                Keys.onEnterPressed: activate()
                Accessible.onPressAction: activate()

                contentItem: ColumnLayout {
                    spacing: Tokens.space["1"]

                    ShellIcons.Icon {
                        objectName: "applicationTileIcon"
                        Layout.alignment: Qt.AlignHCenter
                        name: String(tile.modelData.iconName)
                        size: 28
                        color: tile.enabled ? Tokens.fg.default : Tokens.fg.disabled
                        symbolic: false
                        fallbackText: String(tile.modelData.displayText)
                        Accessible.ignored: true
                    }

                    Text {
                        objectName: "applicationTileLabel"
                        Layout.fillWidth: true
                        text: String(tile.modelData.displayText)
                        color: tile.enabled ? Tokens.fg.default : Tokens.fg.disabled
                        font.family: Tokens.type.fontFamily
                        font.pointSize: Tokens.type.caption
                        horizontalAlignment: Text.AlignHCenter
                        elide: Text.ElideRight
                        maximumLineCount: 2
                        Accessible.ignored: true
                    }
                }

                background: Rectangle {
                    radius: Tokens.radius.m
                    color: tile.down ? Tokens.state.pressed
                         : tile.hovered ? Tokens.state.hover : Tokens.bg.highest
                    border.width: Tokens.space["1"] / 2
                    border.color: Tokens.outline.divider
                    C.FocusRing { anchors.fill: parent; control: tile }
                }
            }
        }
    }
}

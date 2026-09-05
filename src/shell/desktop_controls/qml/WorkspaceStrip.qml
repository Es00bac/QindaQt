// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0 as C
import QindaQt.Shell.Icons 1.0 as ShellIcons
import QindaQt.Tokens 1.0

// Row (or column) of workspace buttons over a WorkspaceController facade.
// Compact mode shows numbers; tile mode shows names. Every gesture re-enters
// the facade with the row's revision so a stale list never switches blindly.
Item {
    id: strip

    required property var access
    property bool vertical: false
    property bool tiles: false
    property int tileExtent: tiles ? 56 : 26

    readonly property bool ready: access !== null && Tokens.ready
    readonly property string phase: ready ? String(access.phaseText) : "unavailable"
    readonly property var rows: ready ? access.rows : []
    readonly property bool showRows: ready && phase === "ready" && rows.length > 0

    implicitWidth: showRows ? layout.implicitWidth : placeholder.implicitWidth
    implicitHeight: showRows ? layout.implicitHeight : placeholder.implicitHeight

    Accessible.role: Accessible.Grouping
    Accessible.name: ready ? String(access.accessibleName) : qsTr("Workspaces")
    Accessible.description: ready ? String(access.accessibleDescription)
                                  : qsTr("Workspace controls are not connected")

    function focusIndex(index) {
        if (index < 0 || index >= repeater.count)
            return
        const item = repeater.itemAt(index)
        if (item !== null)
            item.forceActiveFocus(Qt.TabFocusReason)
    }

    // Unavailable/loading/degraded states stay icon-only in the panel; the
    // accessible description above carries the reason.
    ShellIcons.Icon {
        id: placeholder
        objectName: "workspaceStripPlaceholder"
        visible: !strip.showRows
        anchors.centerIn: parent
        name: "virtual-desktops"
        size: 18
        color: strip.phase === "loading" ? Tokens.fg.muted : Tokens.fg.disabled
        symbolic: true
        fallbackText: qsTr("Workspaces")
        Accessible.ignored: true
    }

    GridLayout {
        id: layout
        anchors.fill: parent
        visible: strip.showRows
        flow: strip.vertical ? GridLayout.TopToBottom : GridLayout.LeftToRight
        rows: strip.vertical ? -1 : 1
        columns: strip.vertical ? 1 : -1
        rowSpacing: Tokens.space["1"]
        columnSpacing: Tokens.space["1"]

        Repeater {
            id: repeater
            model: strip.rows

            delegate: T.Button {
                id: tile

                required property var modelData
                required property int index

                readonly property bool current: Boolean(modelData.current)
                readonly property string name: String(modelData.name)

                objectName: "workspaceTile"
                Layout.preferredWidth: strip.tiles && !strip.vertical
                                       ? Math.max(strip.tileExtent, label.implicitWidth + padding * 2)
                                       : strip.tileExtent
                Layout.preferredHeight: strip.tiles && strip.vertical
                                        ? Math.max(28, label.implicitHeight + padding * 2)
                                        : (strip.tiles ? strip.tileExtent : Math.max(20, strip.height - 4))
                padding: Tokens.space["1"]
                focusPolicy: Qt.TabFocus
                hoverEnabled: true
                enabled: strip.ready && strip.access.canSwitch || tile.current
                checkable: false

                Accessible.role: Accessible.RadioButton
                Accessible.name: String(modelData.accessibleName)
                Accessible.description: tile.current
                                        ? qsTr("Current workspace")
                                        : qsTr("Switch to this workspace")
                Accessible.checked: tile.current

                function activate() {
                    if (strip.ready)
                        strip.access.switchTo(String(modelData.id), modelData.revision)
                }

                onClicked: activate()
                Keys.onReturnPressed: activate()
                Keys.onEnterPressed: activate()
                Accessible.onPressAction: activate()
                Keys.onLeftPressed: if (!strip.vertical) strip.focusIndex(index - 1)
                Keys.onRightPressed: if (!strip.vertical) strip.focusIndex(index + 1)
                Keys.onUpPressed: if (strip.vertical) strip.focusIndex(index - 1)
                Keys.onDownPressed: if (strip.vertical) strip.focusIndex(index + 1)

                contentItem: Text {
                    id: label
                    objectName: "workspaceTileLabel"
                    text: strip.tiles ? tile.name : String(index + 1)
                    color: tile.current ? Tokens.accent.fg
                         : tile.enabled ? Tokens.fg.default : Tokens.fg.disabled
                    font.family: Tokens.type.fontFamily
                    font.pointSize: Tokens.type.caption
                    font.weight: tile.current ? Font.DemiBold : Font.Normal
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                    Accessible.ignored: true
                }

                background: Rectangle {
                    radius: Tokens.radius.s
                    color: tile.current ? Tokens.accent.default
                         : tile.down ? Tokens.state.pressed
                         : tile.hovered ? Tokens.state.hover
                         : Tokens.bg.highest
                    border.width: Tokens.space["1"] / 2
                    border.color: tile.current ? Tokens.accent.default : Tokens.outline.divider

                    C.FocusRing {
                        anchors.fill: parent
                        control: tile
                    }
                }
            }
        }
    }
}

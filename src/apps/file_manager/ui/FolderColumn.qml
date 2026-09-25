// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QindaTK as Tk
import "EntryDrag.js" as EntryDrag
import "EntryText.js" as EntryText

// One read-only column of the Columns view (ADR-0270): a folder above the
// one being browsed, or the contents of a folder selected in it. Its rows are
// ColumnListing's light rows -- names and kinds, no file identity -- so they
// are never dragged or acted on; a click becomes ordinary navigation
// (rowActivated), and a folder row still takes drops into that folder.
ListView {
    id: root

    required property var rows
    required property int rowHeight
    required property int rowIconSize
    // The row on the browsed path, highlighted as Finder does.
    property string highlightPath: ""
    property bool showExtensions: true
    property var mutationController: null
    property var clipboardController: null

    signal rowActivated(var row)

    clip: true
    boundsBehavior: Flickable.StopAtBounds
    model: root.rows
    Accessible.role: Accessible.List
    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

    // Bring the highlighted row into view whenever the rows change.
    onRowsChanged: Qt.callLater(root.revealHighlight)
    Component.onCompleted: root.revealHighlight()
    function revealHighlight() {
        for (let i = 0; i < root.rows.length; ++i) {
            if (root.rows[i].path === root.highlightPath) {
                root.positionViewAtIndex(i, ListView.Contain)
                return
            }
        }
    }

    delegate: Rectangle {
        id: row
        required property var modelData
        required property int index
        readonly property bool onPath: modelData.path === root.highlightPath

        width: ListView.view ? ListView.view.width : 0
        height: root.rowHeight
        color: row.onPath ? Tk.Theme.color.controlActiveBg
             : rowMouse.containsMouse ? Tk.Theme.color.hover : "transparent"
        Accessible.role: Accessible.ListItem
        Accessible.name: row.modelData.name + EntryText.kindSuffix(row.modelData)
        Accessible.selected: row.onPath

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Tk.Theme.space.md
            anchors.rightMargin: Tk.Theme.space.sm
            spacing: Tk.Theme.space.md
            opacity: row.modelData.isHidden === true ? Tk.Theme.opacity.muted : 1
            Image {
                Layout.preferredWidth: root.rowIconSize
                Layout.preferredHeight: root.rowIconSize
                sourceSize: Qt.size(root.rowIconSize, root.rowIconSize)
                source: EntryText.iconUrl(row.modelData, root.rowIconSize)
                Accessible.ignored: true
            }
            Tk.Label {
                Layout.fillWidth: true
                text: EntryText.displayName(row.modelData, root.showExtensions)
                elide: Text.ElideMiddle
                Accessible.ignored: true
            }
            Tk.Icon {
                visible: row.modelData.isDirectory === true
                name: "chevron-right"
                size: Tk.Theme.size.iconSm
                color: Tk.Theme.color.textMuted
            }
        }
        MouseArea {
            id: rowMouse
            anchors.fill: parent
            hoverEnabled: true
            onClicked: root.rowActivated(row.modelData)
        }
        DropArea {
            anchors.fill: parent
            enabled: row.modelData.isDirectory === true
            onEntered: (drag) => drag.accepted = EntryDrag.canAccept(drag)
            onDropped: (drop) => {
                const action = EntryDrag.dispatch(drop, row.modelData.path,
                                                  root.mutationController, root.clipboardController)
                if (action !== Qt.IgnoreAction)
                    drop.accept(action)
                else
                    drop.accepted = false
            }
        }
    }
}

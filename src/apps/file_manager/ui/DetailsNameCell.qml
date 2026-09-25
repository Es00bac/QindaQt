// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QindaTK as Tk
import "EntryDrag.js" as EntryDrag
import "EntryText.js" as EntryText
import QindaQt.Controls 1.0 as C

// The Details view's Name cell (ADR-0270). Tk.DataTable (r5) knows one
// current row and a tap that "activates" it; a file manager needs a
// multi-selection, double-click opening, drag and drop and group headings.
// So the Name column -- always first, always shown -- carries the whole
// row's behaviour in `rowArea`, which spans the row: its selection tint,
// focus outline, heading label, click and drag handling and folder drop
// target. The other cells are plain text on top and let presses through.
//
// AGENT-CONTRACT (Tk.DataTable): a cell delegate reads its data from its
// parent Loader (`parent.row`); here a row is an entry index, or a
// {heading} object for a Group By heading (DetailsView.rowModel).
// AGENT-GUARD: rowArea's x assumes this cell is the row's first, at the
// table's left padding (Tk.Theme.space.sm); DetailsColumnSet keeps Name
// first.
Item {
    id: root

    required property var view

    readonly property var cellRow: parent ? parent.row : undefined
    readonly property bool heading: cellRow !== undefined && cellRow !== null
        && typeof cellRow === "object"
    readonly property int entryIndex: typeof cellRow === "number" ? cellRow : -1
    // A pooled row can briefly outlive a shorter listing: missing is null.
    readonly property var entry: root.entryIndex >= 0
        ? (root.view.selection.entries[root.entryIndex] || null) : null
    readonly property bool entrySelected: root.entryIndex >= 0
        && root.view.selection.isSelected(root.entryIndex)
    readonly property bool dimmed: root.entry !== null
        && (root.entry.isHidden === true || root.entry.launchable === false)

    Item {
        id: rowArea
        x: -Tk.Theme.space.sm
        width: root.view.tableWidth
        height: root.height

        Drag.active: dragHandler.active
        Drag.dragType: Drag.Automatic
        Drag.supportedActions: Qt.CopyAction | Qt.MoveAction
        Drag.mimeData: root.entry === null ? ({})
            : EntryDrag.mimeFor(root.entrySelected ? root.view.selection.selectedEntries() : [root.entry])

        DragHandler {
            id: dragHandler
            target: null
            enabled: root.entry !== null
        }

        Accessible.role: root.heading ? Accessible.Heading : Accessible.ListItem
        Accessible.name: root.heading ? String(root.cellRow.heading)
            : root.entry !== null ? root.entry.name + EntryText.kindSuffix(root.entry) : ""
        // ADR-0262: a dimmed application row also says why in words.
        Accessible.description: root.entry !== null ? (root.entry.note || "") : ""
        Accessible.selected: root.entrySelected

        Rectangle {
            objectName: root.heading ? "detailsHeading" : "detailsRowTint"
            anchors.fill: parent
            color: root.heading ? Tk.Theme.color.headerBg
                 : root.entrySelected ? Tk.Theme.color.selection
                 : rowMouse.containsMouse ? Tk.Theme.color.hover : "transparent"
            border.width: root.entryIndex >= 0 && root.entryIndex === root.view.selection.currentIndex
                && root.view.keyboardFocused ? Tk.Theme.size.focusRing : 0
            border.color: Tk.Theme.color.focus
        }

        Tk.Label {
            visible: root.heading
            anchors.fill: parent
            anchors.leftMargin: Tk.Theme.space.lg
            verticalAlignment: Text.AlignVCenter
            text: root.heading ? String(root.cellRow.heading) : ""
            font.weight: Font.DemiBold
            muted: true
            elide: Text.ElideRight
            Accessible.ignored: true
        }

        MouseArea {
            id: rowMouse
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            ToolTip.visible: containsMouse && root.entry !== null && (root.entry.note || "").length > 0
            ToolTip.text: root.entry !== null ? (root.entry.note || "") : ""
            ToolTip.delay: 600
            onClicked: (mouse) => {
                root.view.focusView()
                if (root.entryIndex < 0)
                    return
                root.view.selection.click(root.entryIndex, mouse.button, mouse.modifiers)
                if (mouse.button === Qt.RightButton)
                    root.view.popupFor(root.view.selection.selectedEntries().length)
            }
            onDoubleClicked: (mouse) => {
                if (root.entryIndex < 0 || mouse.modifiers !== Qt.NoModifier)
                    return
                root.view.selection.selectOnly(root.entryIndex)
                root.view.activateCurrent()
            }
        }

        C.TouchContextArea {
            objectName: "entryTouchContext"
            anchors.fill: parent
            enabled: root.entryIndex >= 0
            onContextRequested: {
                root.view.focusView()
                root.view.selection.target(root.entryIndex)
                root.view.popupFor(root.view.selection.selectedEntries().length)
            }
        }

        DropArea {
            anchors.fill: parent
            enabled: root.entry !== null && root.entry.isDirectory === true
            onEntered: (drag) => drag.accepted = EntryDrag.canAccept(drag)
            onDropped: (drop) => {
                const action = EntryDrag.dispatch(drop, root.entry.path,
                                                  root.view.mutationController,
                                                  root.view.clipboardController)
                if (action !== Qt.IgnoreAction)
                    drop.accept(action)
                else
                    drop.accepted = false
            }
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: Tk.Theme.space.md
        visible: root.entry !== null
        opacity: root.dimmed ? Tk.Theme.opacity.muted : 1

        Image {
            Layout.preferredWidth: root.view.rowIconSize
            Layout.preferredHeight: root.view.rowIconSize
            sourceSize: Qt.size(root.view.rowIconSize, root.view.rowIconSize)
            source: root.entry !== null ? EntryText.iconUrl(root.entry, root.view.rowIconSize) : ""
            Accessible.ignored: true
            Image {
                visible: root.entry !== null && root.entry.isSymlink === true
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                width: Math.round(parent.width / 2)
                height: width
                sourceSize: Qt.size(width, height)
                source: "image://theme-icons/emblem-symbolic-link"
                Accessible.ignored: true
            }
        }
        Tk.Label {
            Layout.fillWidth: true
            text: root.entry !== null ? EntryText.displayName(root.entry, root.view.showExtensions) : ""
            elide: Text.ElideMiddle
            verticalAlignment: Text.AlignVCenter
            Accessible.ignored: true
        }
    }
}

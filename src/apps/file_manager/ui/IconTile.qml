// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QindaTK as Tk
import "EntryDrag.js" as EntryDrag
import "EntryText.js" as EntryText
import QindaQt.Controls 1.0 as C

// One Icons-view entry (ADR-0270): a Tk.Thumbnail tile and the entry's name.
// The tile's picture is the theme icon; a real preview from the bounded
// pipeline (ADR-0111) covers it once one has decoded, so an image that cannot
// be previewed keeps its recognisable icon. Extracted from IconsView so both
// stay within the source-shape budget; the view injects everything.
Item {
    id: root

    required property var modelData
    required property int index
    required property var selection
    // The owning GridView: focus goes there, and it says which tile is current.
    required property var view
    required property int iconSize
    required property bool showExtensions
    // Optional drop dispatch targets (fixture views may leave them null).
    property var mutationController: null
    property var clipboardController: null

    signal activated()
    signal contextMenuRequested()

    readonly property bool entrySelected: root.selection.isSelected(root.index)
    readonly property bool dimmed: root.modelData.isHidden === true
        || root.modelData.launchable === false

    width: root.view.cellWidth - 4
    height: root.view.cellHeight - 4

    Drag.active: dragHandler.active
    Drag.dragType: Drag.Automatic
    Drag.supportedActions: Qt.CopyAction | Qt.MoveAction
    Drag.mimeData: EntryDrag.mimeFor(root.entrySelected ? root.selection.selectedEntries()
                                                        : [root.modelData])

    DragHandler {
        id: dragHandler
        target: null
    }

    Accessible.role: Accessible.ListItem
    Accessible.name: root.modelData.name + EntryText.kindSuffix(root.modelData)
    // ADR-0262: a dimmed application says why in words too.
    Accessible.description: root.modelData.note || ""
    Accessible.selected: root.entrySelected

    // Hover tint, and the focus outline that follows the keyboard's current
    // entry, selected or not (Ctrl+arrows move it alone).
    Rectangle {
        anchors.fill: parent
        radius: Tk.Theme.radius.lg
        color: hoverArea.containsMouse ? Tk.Theme.color.hover : "transparent"
        border.width: root.GridView.isCurrentItem && root.view.activeFocus
            ? Tk.Theme.size.focusRing : 0
        border.color: Tk.Theme.color.focus
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 6
        spacing: 4

        Tk.Thumbnail {
            id: tile
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: root.iconSize + 16
            Layout.preferredHeight: root.iconSize + 16
            source: EntryText.iconUrl(root.modelData, root.iconSize)
            placeholderIcon: EntryText.glyphFor(root.modelData)
            crop: false
            selected: root.entrySelected
            opacity: root.dimmed ? Tk.Theme.opacity.muted : 1
            Accessible.ignored: true

            // A refused preview arrives as a 1x1 image and leaves the icon.
            Rectangle {
                anchors.fill: parent
                anchors.margins: tile.border.width
                radius: Tk.Theme.radius.sm
                color: Tk.Theme.color.canvas
                visible: preview.status === Image.Ready && preview.implicitWidth > 1
                Image {
                    id: preview
                    anchors.fill: parent
                    source: root.modelData.previewUrl || ""
                    fillMode: Image.PreserveAspectFit
                    asynchronous: true
                    cache: false
                    smooth: true
                    Accessible.ignored: true
                }
            }
            Image {
                visible: root.modelData.isSymlink === true
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: 2
                width: 20
                height: 20
                sourceSize: Qt.size(20, 20)
                source: "image://theme-icons/emblem-symbolic-link"
                Accessible.ignored: true
            }
        }

        // Finder's name label: an accent pill while selected.
        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            Layout.maximumWidth: parent.width
            implicitWidth: nameLabel.implicitWidth + 12
            implicitHeight: nameLabel.implicitHeight + 4
            radius: Tk.Theme.radius.sm
            color: root.entrySelected ? Tk.Theme.color.accent : "transparent"
            Label {
                id: nameLabel
                anchors.fill: parent
                anchors.leftMargin: 6
                anchors.rightMargin: 6
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                text: EntryText.displayName(root.modelData, root.showExtensions)
                color: root.entrySelected ? Tk.Theme.color.accentContrast
                     : root.dimmed ? Tk.Theme.color.textMuted : Tk.Theme.color.text
                elide: Text.ElideMiddle
                maximumLineCount: 2
                wrapMode: Text.Wrap
                Accessible.ignored: true
            }
        }
        Item { Layout.fillHeight: true }
    }

    MouseArea {
        id: hoverArea
        anchors.fill: parent
        hoverEnabled: true
        ToolTip.visible: containsMouse && (root.modelData.note || "").length > 0
        ToolTip.text: root.modelData.note || ""
        ToolTip.delay: 600
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onClicked: (mouse) => {
            root.view.forceActiveFocus()
            root.selection.click(root.index, mouse.button, mouse.modifiers)
            if (mouse.button === Qt.RightButton)
                root.contextMenuRequested()
        }
        onDoubleClicked: (mouse) => {
            if (mouse.modifiers !== Qt.NoModifier)
                return
            root.selection.selectOnly(root.index)
            root.activated()
        }
    }

    C.TouchContextArea {
        objectName: "entryTouchContext"
        anchors.fill: parent
        onContextRequested: {
            root.view.forceActiveFocus()
            root.selection.target(root.index)
            root.contextMenuRequested()
        }
    }

    DropArea {
        anchors.fill: parent
        enabled: root.modelData.isDirectory === true
        onEntered: (drag) => drag.accepted = EntryDrag.canAccept(drag)
        onDropped: (drop) => {
            const action = EntryDrag.dispatch(drop, root.modelData.path,
                                              root.mutationController, root.clipboardController)
            if (action !== Qt.IgnoreAction)
                drop.accept(action)
            else
                drop.accepted = false
        }
    }
}

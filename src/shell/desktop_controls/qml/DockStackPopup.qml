// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0 as C
import QindaQt.Shell.Icons 1.0 as ShellIcons
import QindaQt.Tokens 1.0
import "DockDropGeometry.js" as DockDrop

// The list a group or a pinned folder unfolds from the dock (ADR-0265):
// group members, or the folder's first children plus "Open in File Manager"
// (macOS stacks). It uses the panels' own popup (ControlPopupFrame over
// PanelPopup), so it opens away from the dock's edge, closes on Escape or a
// press outside, and takes keyboard focus into its first row. The rows
// unfold by sliding out of the dock edge inside a fixed-size window: a
// Wayland popup that resized while opening would jump.
//
// AGENT-NOTE: members can be dragged back onto the dock (memberFormat, see
// DockDropGeometry.js); the dock's DropArea turns that into moveOutOfGroup.
ControlPopupFrame {
    id: popup

    required property var access
    property bool reducedMotion: false
    // The row this popup shows. Its `stackKey` survives dock rebuilds (a
    // running window changes the rows, not the group); the applet re-finds
    // the row by key and closes the popup when it is gone.
    property var stackRow: ({})
    property var entries: []
    property real reveal: 1
    // The rows the dock shows now, and its tile lookup (position -> Item).
    // The popup follows its item across dock rebuilds (a window opening
    // changes the rows, not the group) and closes when the item is gone.
    property var dockRows: []
    property var tileAt: null

    readonly property bool isGroup: String(stackRow.kind ?? "") === "group"
    readonly property var stackItems: isGroup ? (stackRow.members ?? []) : entries
    readonly property int unfoldDirection: resolvedPanelEdge === "top" ? -1 : 1

    // Closing returns keyboard focus to the tile the stack unfolded from, so
    // Escape leaves a keyboard user where they were. It only sets the panel
    // window's focus item; it never activates the panel window.
    onClosed: if (anchorItem !== null) anchorItem.forceActiveFocus(Qt.PopupFocusReason)

    function show(row, anchor) {
        stackRow = row
        entries = String(row.kind ?? "") === "folder" && access !== null
            ? access.folderEntries(Number(row.index)) : []
        anchorItem = anchor
        open()
    }

    function stackKey(row) {
        const kind = String(row.kind ?? "")
        return kind === "group" ? "group:" + String(row.displayText)
             : kind === "folder" ? "folder:" + String(row.path) : ""
    }

    onDockRowsChanged: {
        if (!opened)
            return
        const key = stackKey(stackRow)
        for (let position = 0; position < dockRows.length; ++position) {
            if (stackKey(dockRows[position]) === key) {
                stackRow = dockRows[position]
                // The dock's Repeater rebuilds its tiles for the new rows;
                // follow the item to its new tile once that has happened.
                Qt.callLater(popup.reanchor, position)
                return
            }
        }
        close()
    }

    function reanchor(position) {
        const tile = tileAt !== null ? tileAt(position) : null
        if (opened && tile !== null)
            anchorItem = tile
    }

    function focusRow(position) {
        const item = rowsRepeater.itemAt(position)
        if (item !== null)
            item.forceActiveFocus(Qt.TabFocusReason)
    }

    objectName: "quickLaunchStackPopup"
    heading: String(stackRow.displayText ?? "")
    feedback: access !== null ? String(access.feedback) : ""
    initialFocusItem: rowsRepeater.count > 0 ? rowsRepeater.itemAt(0) : openInManager

    NumberAnimation {
        id: unfold
        target: popup
        property: "reveal"
        from: 0
        to: 1
        duration: Tokens.motion.short
        easing.type: Easing.OutCubic
    }

    Connections {
        target: popup
        function onAboutToShow() {
            if (popup.reducedMotion) {
                popup.reveal = 1
            } else {
                popup.reveal = 0
                unfold.restart()
            }
        }
    }

    Item {
        id: fold
        Layout.fillWidth: true
        implicitWidth: 280
        implicitHeight: list.implicitHeight
        clip: true

        Column {
            id: list
            width: fold.width
            spacing: Tokens.space["1"]
            y: (1 - popup.reveal) * popup.unfoldDirection * fold.height
            opacity: popup.reveal

            Repeater {
                id: rowsRepeater
                model: popup.stackItems

                T.ItemDelegate {
                    id: stackRowItem

                    required property var modelData
                    required property int index

                    readonly property string label: popup.isGroup
                        ? String(modelData.displayText ?? "") : String(modelData.name ?? "")

                    function activate() {
                        if (popup.access === null)
                            return
                        const done = popup.isGroup
                            ? popup.access.activate(String(modelData.entryId))
                            : popup.access.openFolderEntry(modelData)
                        if (done)
                            popup.close()
                    }

                    objectName: "quickLaunchStackRow"
                    width: list.width
                    implicitHeight: 36
                    text: label
                    focusPolicy: Qt.StrongFocus
                    hoverEnabled: true
                    leftPadding: Tokens.space["2"]
                    rightPadding: Tokens.space["2"]
                    Accessible.role: Accessible.ListItem
                    Accessible.name: label
                    Accessible.description: popup.isGroup
                        ? (Boolean(modelData.running) ? qsTr("Running") : "")
                        : (Boolean(modelData.isDirectory) ? qsTr("Folder") : qsTr("File"))

                    onClicked: activate()
                    Keys.onReturnPressed: activate()
                    Keys.onEnterPressed: activate()
                    Keys.onSpacePressed: activate()
                    Keys.onUpPressed: popup.focusRow(index - 1)
                    Keys.onDownPressed: {
                        if (index + 1 < rowsRepeater.count)
                            popup.focusRow(index + 1)
                        else if (openInManager.visible)
                            openInManager.forceActiveFocus(Qt.TabFocusReason)
                    }
                    Keys.onPressed: (event) => {
                        if (popup.isGroup && (event.key === Qt.Key_Menu
                                || (event.key === Qt.Key_F10
                                    && (event.modifiers & Qt.ShiftModifier)))) {
                            memberMenu.openFor(modelData, stackRowItem)
                            event.accepted = true
                        }
                    }
                    Accessible.onPressAction: activate()

                    // Group members drag back onto the dock; folder children
                    // never enter a drag here (the File Manager owns files).
                    Drag.active: memberDrag.active
                    Drag.dragType: Drag.Automatic
                    Drag.supportedActions: Qt.MoveAction
                    Drag.mimeData: popup.isGroup
                        ? ({ [DockDrop.memberFormat]: JSON.stringify({
                               "group": Number(popup.stackRow.index),
                               "entryId": String(modelData.entryId) }) })
                        : ({})

                    DragHandler {
                        id: memberDrag
                        target: null
                        enabled: popup.isGroup
                    }

                    MouseArea {
                        anchors.fill: parent
                        acceptedButtons: Qt.RightButton
                        enabled: popup.isGroup
                        onClicked: memberMenu.openFor(stackRowItem.modelData, stackRowItem)
                    }

                    contentItem: RowLayout {
                        spacing: Tokens.space["2"]

                        ShellIcons.Icon {
                            name: String(stackRowItem.modelData.iconName ?? "")
                            size: 24
                            symbolic: false
                            fallbackText: stackRowItem.label
                            Accessible.ignored: true
                        }
                        C.Label {
                            Layout.fillWidth: true
                            text: stackRowItem.label
                            elide: Text.ElideRight
                            Accessible.ignored: true
                        }
                        // Running is also said in the accessible description.
                        Rectangle {
                            visible: Boolean(stackRowItem.modelData.running)
                            implicitWidth: 6
                            implicitHeight: 6
                            radius: 3
                            color: Tokens.fg.muted
                            Accessible.ignored: true
                        }
                    }

                    background: Rectangle {
                        radius: Tokens.radius.m
                        color: stackRowItem.down ? Tokens.state.pressed
                             : stackRowItem.hovered ? Tokens.state.hover : "transparent"
                        C.FocusRing { anchors.fill: parent; control: stackRowItem }
                    }
                }
            }
        }
    }

    C.Label {
        objectName: "quickLaunchStackEmpty"
        Layout.fillWidth: true
        visible: !popup.isGroup && popup.stackItems.length === 0
        text: qsTr("This folder is empty or cannot be read")
        muted: true
    }

    C.Button {
        id: openInManager
        objectName: "quickLaunchStackOpenInFileManager"
        Layout.alignment: Qt.AlignRight
        visible: !popup.isGroup
        text: qsTr("Open in File Manager")
        emphasized: false
        onClicked: {
            if (popup.access !== null
                    && popup.access.openInFileManager(Number(popup.stackRow.index)))
                popup.close()
        }
        Keys.onUpPressed: popup.focusRow(rowsRepeater.count - 1)
    }

    T.Menu {
        id: memberMenu
        objectName: "quickLaunchStackMemberMenu"
        popupType: T.Popup.Window

        property var member: ({})

        function openFor(member, item) {
            memberMenu.member = member
            memberMenu.popup(item)
        }

        T.MenuItem {
            objectName: "quickLaunchStackRemoveFromGroup"
            text: qsTr("Remove from Group")
            onTriggered: {
                // The member lands right after its group on the dock.
                const group = Number(popup.stackRow.index)
                popup.access.moveOutOfGroup(group, String(memberMenu.member.entryId), group + 1)
            }
        }
        T.MenuItem {
            objectName: "quickLaunchStackRemoveFromDock"
            text: qsTr("Remove from Dock")
            onTriggered: popup.access.unpin(String(memberMenu.member.entryId))
        }
    }
}

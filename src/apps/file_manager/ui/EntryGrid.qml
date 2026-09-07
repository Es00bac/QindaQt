// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as T
import QindaQt.Tokens 1.0
import QindaQt.Controls 1.0 as Qinda

// Grid presentation over the same published listing and selection contract as
// EntryList. Glyphs are deliberately built-in minimal shapes; XDG icon-theme
// and MIME-aware icons are an S3 outcome pending an icon-seam ADR.
Item {
    id: root

    required property var navigationController
    required property var appCoordinator

    function currentEntry() {
        return gridView.currentIndex >= 0
            ? root.navigationController.entries[gridView.currentIndex] : null
    }

    function selectedEntries() {
        const selected = selection.selectedEntries()
        if (selected.length > 0)
            return selected
        const current = currentEntry()
        return current ? [current] : []
    }

    function selectAll() {
        selection.selectAll()
    }

    function activateCurrent() {
        if (gridView.currentIndex >= 0) {
            root.navigationController.activate(gridView.currentIndex)
        }
    }

    property string lastSelectedName: ""

    EntrySelection {
        id: selection
        navigationController: root.navigationController
    }

    Connections {
        target: root.navigationController
        function onEntriesChanged() {
            selection.prune()
            const restored = root.navigationController.indexOfName(root.lastSelectedName)
            gridView.currentIndex = restored >= 0
                ? restored
                : (root.navigationController.entries.length > 0 ? 0 : -1)
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Tokens.space["2"]
        spacing: 0

        GridView {
            id: gridView
            objectName: "entryGridView"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            focus: true
            keyNavigationEnabled: true
            cellWidth: 112
            cellHeight: 104
            model: root.navigationController.entries

            Accessible.role: Accessible.List
            Accessible.name: qsTr("Folder contents")

            onCurrentIndexChanged: {
                root.lastSelectedName = currentIndex >= 0
                    ? root.navigationController.entries[currentIndex].name : ""
            }

            Keys.onReturnPressed: root.activateCurrent()
            Keys.onEnterPressed: root.activateCurrent()
            Keys.onPressed: (event) => {
                if (event.key === Qt.Key_Backspace) {
                    root.navigationController.goUp()
                    event.accepted = true
                } else if (event.key === Qt.Key_Menu
                           || (event.key === Qt.Key_F10
                               && (event.modifiers & Qt.ShiftModifier))) {
                    contextMenu.popup()
                    event.accepted = true
                }
            }

            delegate: Rectangle {
                id: delegateRoot

                required property var modelData
                required property int index

                property bool entrySelected: selection.isSelected(delegateRoot.index)

                width: gridView.cellWidth - Tokens.space["1"]
                height: gridView.cellHeight - Tokens.space["1"]
                radius: 4
                color: delegateRoot.entrySelected ? Tokens.state.pressed
                     : GridView.isCurrentItem ? Tokens.state.pressed
                     : hoverArea.containsMouse ? Tokens.state.hover : "transparent"

                Accessible.role: Accessible.ListItem
                Accessible.name: delegateRoot.modelData.name + (delegateRoot.modelData.isDirectory
                    ? qsTr(", folder") : qsTr(", file"))
                Accessible.selected: delegateRoot.entrySelected || GridView.isCurrentItem

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Tokens.space["2"]
                    spacing: Tokens.space["1"]

                    Item {
                        Layout.alignment: Qt.AlignHCenter
                        Layout.preferredWidth: 40
                        Layout.preferredHeight: 40

                        // Folder: tabbed outline. File: folded-corner page.
                        // Symlink: accent badge dot over the base glyph.
                        Rectangle {
                            visible: delegateRoot.modelData.isDirectory
                            anchors.fill: parent
                            radius: 4
                            color: Tokens.accent.default
                            Rectangle {
                                width: 18
                                height: 7
                                radius: 2
                                color: parent.color
                                anchors.left: parent.left
                                anchors.top: parent.top
                                anchors.leftMargin: 2
                                anchors.topMargin: -3
                            }
                        }
                        Rectangle {
                            visible: !delegateRoot.modelData.isDirectory
                            anchors.fill: parent
                            radius: 2
                            color: "transparent"
                            border.color: Tokens.fg.muted
                            border.width: 2
                        }
                        Rectangle {
                            visible: delegateRoot.modelData.isSymlink
                            width: 12
                            height: 12
                            radius: 6
                            color: Tokens.accent.default
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                        }
                        Accessible.ignored: true
                    }

                    Qinda.Label {
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                        text: delegateRoot.modelData.name
                        muted: delegateRoot.modelData.isHidden
                        elide: Text.ElideMiddle
                        maximumLineCount: 2
                        wrapMode: Text.Wrap
                        Accessible.ignored: true
                    }
                }

                MouseArea {
                    id: hoverArea
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    onClicked: (mouse) => {
                        if (mouse.modifiers & Qt.ControlModifier) {
                            selection.toggle(delegateRoot.index)
                            gridView.currentIndex = delegateRoot.index
                        } else if (mouse.modifiers & Qt.ShiftModifier) {
                            selection.rangeTo(delegateRoot.index)
                            gridView.currentIndex = delegateRoot.index
                        } else {
                            selection.selectOnly(delegateRoot.index)
                            gridView.currentIndex = delegateRoot.index
                        }
                        if (mouse.button === Qt.RightButton)
                            contextMenu.popup()
                    }
                    onDoubleClicked: (mouse) => {
                        if (mouse.modifiers !== Qt.NoModifier)
                            return
                        selection.selectOnly(delegateRoot.index)
                        gridView.currentIndex = delegateRoot.index
                        root.activateCurrent()
                    }
                }
            }
        }

        T.Menu {
            id: contextMenu

            T.MenuItem {
                text: qsTr("Rename")
                onTriggered: root.appCoordinator.activateAction("file.rename")
            }
            T.MenuItem {
                text: qsTr("Copy To…")
                onTriggered: root.appCoordinator.activateAction("file.copy")
            }
            T.MenuItem {
                text: qsTr("Move To…")
                onTriggered: root.appCoordinator.activateAction("file.move")
            }
            T.MenuItem {
                text: qsTr("Move to Trash")
                onTriggered: root.appCoordinator.activateAction("file.trash")
            }
        }

        Qinda.Label {
            Layout.fillWidth: true
            Layout.topMargin: Tokens.space["1"]
            visible: root.navigationController.statusMessage.length > 0
            text: root.navigationController.statusMessage
            muted: true
        }
    }
}

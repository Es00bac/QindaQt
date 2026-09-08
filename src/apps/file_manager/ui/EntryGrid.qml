// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as T
import QindaQt.Tokens 1.0
import QindaQt.Controls 1.0 as Qinda

// Visual browsing shares the exact selection policy with the details view.
Item {
    id: root

    required property var navigationController
    required property var selection
    required property var appCoordinator

    function focusView() { gridView.forceActiveFocus() }

    function currentEntry() {
        return gridView.currentIndex >= 0
            ? root.navigationController.entries[gridView.currentIndex] : null
    }

    function selectedEntries() {
        return selection.selectedEntries()
    }

    function selectAll() {
        selection.selectAll()
    }

    function activateCurrent() {
        if (gridView.currentIndex >= 0) {
            root.navigationController.activate(gridView.currentIndex)
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
            keyNavigationEnabled: false
            currentIndex: root.selection.currentIndex
            function selectEntry(index) { root.selection.selectOnly(index) }
            cellWidth: Math.max(116, Math.floor(width / Math.max(1, Math.floor(width / 136))))
            cellHeight: 144
            model: root.navigationController.entries

            Accessible.role: Accessible.List
            Accessible.name: qsTr("Folder contents")

            Keys.onReturnPressed: root.activateCurrent()
            Keys.onEnterPressed: root.activateCurrent()
            Keys.onPressed: (event) => {
                if (event.key === Qt.Key_Backspace) {
                    root.navigationController.goUp()
                    event.accepted = true
                } else if ([Qt.Key_Up, Qt.Key_Down, Qt.Key_Left, Qt.Key_Right,
                            Qt.Key_Home, Qt.Key_End].indexOf(event.key) >= 0) {
                    const columns = Math.max(1, Math.floor(gridView.width / gridView.cellWidth))
                    let target = root.selection.currentIndex
                    if (event.key === Qt.Key_Home) target = 0
                    else if (event.key === Qt.Key_End) target = gridView.count - 1
                    else if (event.key === Qt.Key_Up) target -= columns
                    else if (event.key === Qt.Key_Down) target += columns
                    else if (event.key === Qt.Key_Left) target -= 1
                    else target += 1
                    target = Math.max(0, Math.min(gridView.count - 1, target))
                    root.selection.moveTo(target, event.modifiers)
                    gridView.positionViewAtIndex(target, gridView.Contain)
                    event.accepted = true
                } else if (event.key === Qt.Key_Space) {
                    if (event.modifiers & Qt.ControlModifier)
                        root.selection.toggle(root.selection.currentIndex)
                    else root.selection.selectOnly(root.selection.currentIndex)
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
                radius: 12
                color: delegateRoot.entrySelected ? Tokens.state.pressed
                     : hoverArea.containsMouse ? Tokens.state.hover : "transparent"

                Accessible.role: Accessible.ListItem
                Accessible.name: delegateRoot.modelData.name + (delegateRoot.modelData.isDirectory
                    ? qsTr(", folder") : qsTr(", file"))
                Accessible.selected: delegateRoot.entrySelected
                border.width: GridView.isCurrentItem && gridView.activeFocus ? 2 : 0
                border.color: Tokens.accent.default

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Tokens.space["2"]
                    spacing: Tokens.space["1"]

                    Item {
                        Layout.alignment: Qt.AlignHCenter
                        Layout.preferredWidth: 80
                        Layout.preferredHeight: 80
                        Qinda.Icon {
                            anchors.centerIn: parent
                            width: 64
                            height: 64
                            name: delegateRoot.modelData.iconName || "application-octet-stream"
                            visible: previewImage.status !== Image.Ready || previewImage.implicitWidth <= 1
                        }
                        Image {
                            id: previewImage
                            anchors.fill: parent
                            source: delegateRoot.modelData.previewUrl || ""
                            fillMode: Image.PreserveAspectFit
                            asynchronous: true
                            cache: false
                            smooth: true
                            Accessible.ignored: true
                        }
                        Qinda.Icon {
                            visible: delegateRoot.modelData.isSymlink
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            width: 20
                            height: 20
                            name: "emblem-symbolic-link"
                            color: Tokens.fg.default
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
                        gridView.forceActiveFocus()
                        if (mouse.button === Qt.RightButton && selection.isSelected(delegateRoot.index)) {
                            selection.focusIndex(delegateRoot.index)
                        } else if (mouse.modifiers & Qt.ControlModifier) {
                            selection.toggle(delegateRoot.index)
                            selection.focusIndex(delegateRoot.index)
                        } else if (mouse.modifiers & Qt.ShiftModifier) {
                            selection.rangeTo(delegateRoot.index)
                            selection.focusIndex(delegateRoot.index)
                        } else {
                            selection.selectOnly(delegateRoot.index)
                            selection.focusIndex(delegateRoot.index)
                        }
                        if (mouse.button === Qt.RightButton)
                            contextMenu.popup()
                    }
                    onDoubleClicked: (mouse) => {
                        if (mouse.modifiers !== Qt.NoModifier)
                            return
                        selection.selectOnly(delegateRoot.index)
                        selection.focusIndex(delegateRoot.index)
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

// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as T
import QindaQt.Tokens 1.0
import QindaQt.Controls 1.0 as Qinda

Item {
    id: root

    required property var navigationController
    required property var selection
    required property var appCoordinator

    function focusView() { listView.forceActiveFocus() }

    function currentEntry() {
        return listView.currentIndex >= 0
            ? root.navigationController.entries[listView.currentIndex] : null
    }

    function selectedEntries() {
        return selection.selectedEntries()
    }

    function selectAll() {
        selection.selectAll()
    }

    function activateCurrent() {
        if (listView.currentIndex >= 0) {
            root.navigationController.activate(listView.currentIndex)
        }
    }

    // Listing notices remain outside the scrolling entries.
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Tokens.space["2"]
        spacing: 0

        RowLayout {
            id: headerRow
            Layout.fillWidth: true
            spacing: 0

            // AGENT-NOTE: The sort headers are statically declared buttons, not
            // a Repeater, because model-instantiated delegates are not
            // reachable through QObject::findChild from the QML root; the
            // --check-ui-contract gate and the UI action probe resolve
            // sortHeader_* by objectName. The column set is fixed by
            // ListingOrder (model/listing_order.h).
            component SortHeaderButton: Qinda.Button {
                required property string key
                required property string label
                property bool stretch: false

                Layout.fillWidth: stretch
                Layout.preferredWidth: stretch ? -1 : 112
                text: label + (root.navigationController.sortColumn === key
                    ? (root.navigationController.sortDirection === "ascending" ? " ▲" : " ▼") : "")
                emphasized: root.navigationController.sortColumn === key
                accessibleDescription: qsTr("Sort by %1").arg(label)
                onClicked: root.navigationController.setSortColumn(key)
            }

            SortHeaderButton {
                objectName: "sortHeader_name"
                key: "name"
                label: qsTr("Name")
                stretch: true
            }
            SortHeaderButton {
                objectName: "sortHeader_size"
                key: "size"
                label: qsTr("Size")
            }
            SortHeaderButton {
                objectName: "sortHeader_kind"
                visible: root.width > 580
                key: "kind"
                label: qsTr("Kind")
            }
            SortHeaderButton {
                objectName: "sortHeader_modified"
                visible: root.width > 440
                key: "modified"
                label: qsTr("Modified")
            }
        }

        ListView {
            id: listView
            objectName: "entryListView"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            focus: true
            keyNavigationEnabled: false
            currentIndex: root.selection.currentIndex
            function selectEntry(index) { root.selection.selectOnly(index) }
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
                    const columns = 1
                    let target = root.selection.currentIndex
                    if (event.key === Qt.Key_Home) target = 0
                    else if (event.key === Qt.Key_End) target = listView.count - 1
                    else if (event.key === Qt.Key_Up) target -= columns
                    else if (event.key === Qt.Key_Down) target += columns
                    else if (event.key === Qt.Key_Left) target -= 1
                    else target += 1
                    target = Math.max(0, Math.min(listView.count - 1, target))
                    root.selection.moveTo(target, event.modifiers)
                    listView.positionViewAtIndex(target, listView.Contain)
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

                width: listView.width
                height: 44
                radius: 8
                color: delegateRoot.entrySelected ? Tokens.state.pressed
                     : hoverArea.containsMouse ? Tokens.state.hover : "transparent"

                Accessible.role: Accessible.ListItem
                Accessible.name: delegateRoot.modelData.name + (delegateRoot.modelData.isDirectory
                    ? qsTr(", folder") : qsTr(", file"))
                Accessible.selected: delegateRoot.entrySelected
                border.width: ListView.isCurrentItem && listView.activeFocus ? 2 : 0
                border.color: Tokens.accent.default

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Tokens.space["3"]
                    anchors.rightMargin: Tokens.space["3"]
                    spacing: Tokens.space["2"]

                    Qinda.Icon {
                        Layout.preferredWidth: 28
                        Layout.preferredHeight: 28
                        name: delegateRoot.modelData.iconName || "application-octet-stream"
                    }
                    Qinda.Label {
                        Layout.fillWidth: true
                        text: delegateRoot.modelData.name
                        muted: delegateRoot.modelData.isHidden
                        elide: Text.ElideMiddle
                        Accessible.ignored: true
                    }
                    Qinda.Label {
                        Layout.preferredWidth: 102
                        horizontalAlignment: Text.AlignRight
                        text: delegateRoot.modelData.sizeText
                        muted: true
                        elide: Text.ElideRight
                        Accessible.ignored: true
                    }
                    Qinda.Label {
                        Layout.preferredWidth: 102
                        visible: root.width > 580
                        text: delegateRoot.modelData.kindText
                        muted: true
                        elide: Text.ElideRight
                        Accessible.ignored: true
                    }
                    Qinda.Label {
                        Layout.preferredWidth: 102
                        visible: root.width > 440
                        text: delegateRoot.modelData.modifiedText
                        muted: true
                        elide: Text.ElideRight
                        Accessible.ignored: true
                    }
                }

                MouseArea {
                    id: hoverArea
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    onClicked: (mouse) => {
                        listView.forceActiveFocus()
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
            objectName: "entryContextMenu"

            T.MenuItem {
                objectName: "contextRenameAction"
                text: qsTr("Rename")
                onTriggered: root.appCoordinator.activateAction("file.rename")
            }
            T.MenuItem {
                objectName: "contextCopyAction"
                text: qsTr("Copy To…")
                onTriggered: root.appCoordinator.activateAction("file.copy")
            }
            T.MenuItem {
                objectName: "contextMoveAction"
                text: qsTr("Move To…")
                onTriggered: root.appCoordinator.activateAction("file.move")
            }
            T.MenuItem {
                objectName: "contextTrashAction"
                text: qsTr("Move to Trash")
                onTriggered: root.appCoordinator.activateAction("file.trash")
            }
        }

        Qinda.Label {
            objectName: "truncationNotice"
            Layout.fillWidth: true
            Layout.topMargin: Tokens.space["1"]
            visible: root.navigationController.statusMessage.length > 0
            text: root.navigationController.statusMessage
            muted: true
        }
    }
}

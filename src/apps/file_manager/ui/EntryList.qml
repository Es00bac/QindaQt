// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as T
import QindaQt.Tokens 1.0
import QindaQt.Controls 1.0 as Qinda

Item {
    id: root

    required property var navigationController
    required property var appCoordinator

    function currentEntry() {
        return listView.currentIndex >= 0
            ? root.navigationController.entries[listView.currentIndex] : null
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
        if (listView.currentIndex >= 0) {
            root.navigationController.activate(listView.currentIndex)
        }
    }

    // AGENT-GUARD: Preserve the previously selected entry's name across a
    // refresh of the same folder so review-visible selection stays
    // deterministic instead of silently jumping to index 0 whenever the
    // underlying listing is rebuilt. A genuine navigation to a different
    // folder still lands on index 0 because the old name normally will not
    // exist there.
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
            listView.currentIndex = restored >= 0
                ? restored
                : (root.navigationController.entries.length > 0 ? 0 : -1)
        }
    }

    // AGENT-NOTE: The controller publishes a non-empty statusMessage only for
    // a truncated ready-state listing or a hidden-entry filter notice (errors
    // use StatePane instead), so this label is the single user-visible surface
    // for those bounds. Keep it outside the ListView so the notice cannot
    // scroll out of view.
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
                Layout.preferredWidth: stretch ? -1 : 140
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
                key: "kind"
                label: qsTr("Kind")
            }
            SortHeaderButton {
                objectName: "sortHeader_modified"
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
            keyNavigationEnabled: true
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
                } else if ((event.key === Qt.Key_Up || event.key === Qt.Key_Down)
                           && (event.modifiers & Qt.ShiftModifier)) {
                    const delta = event.key === Qt.Key_Up ? -1 : 1
                    const target = Math.max(0, Math.min(listView.count - 1,
                                                        listView.currentIndex + delta))
                    if (target !== listView.currentIndex) {
                        listView.currentIndex = target
                        selection.rangeTo(target)
                    }
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
                height: 36
                color: delegateRoot.entrySelected ? Tokens.state.pressed
                     : ListView.isCurrentItem ? Tokens.state.pressed
                     : hoverArea.containsMouse ? Tokens.state.hover : "transparent"

                Accessible.role: Accessible.ListItem
                Accessible.name: delegateRoot.modelData.name + (delegateRoot.modelData.isDirectory
                    ? qsTr(", folder") : qsTr(", file"))
                Accessible.selected: delegateRoot.entrySelected || ListView.isCurrentItem

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Tokens.space["3"]
                    anchors.rightMargin: Tokens.space["3"]
                    spacing: Tokens.space["2"]

                    Qinda.Label {
                        Layout.fillWidth: true
                        text: delegateRoot.modelData.name
                        muted: delegateRoot.modelData.isHidden
                        elide: Text.ElideMiddle
                        Accessible.ignored: true
                    }
                    Qinda.Label {
                        Layout.preferredWidth: 130
                        horizontalAlignment: Text.AlignRight
                        text: delegateRoot.modelData.sizeText
                        muted: true
                        elide: Text.ElideRight
                        Accessible.ignored: true
                    }
                    Qinda.Label {
                        Layout.preferredWidth: 130
                        text: delegateRoot.modelData.kindText
                        muted: true
                        elide: Text.ElideRight
                        Accessible.ignored: true
                    }
                    Qinda.Label {
                        Layout.preferredWidth: 130
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
                        if (mouse.modifiers & Qt.ControlModifier) {
                            selection.toggle(delegateRoot.index)
                            listView.currentIndex = delegateRoot.index
                        } else if (mouse.modifiers & Qt.ShiftModifier) {
                            selection.rangeTo(delegateRoot.index)
                            listView.currentIndex = delegateRoot.index
                        } else {
                            selection.selectOnly(delegateRoot.index)
                            listView.currentIndex = delegateRoot.index
                        }
                        if (mouse.button === Qt.RightButton)
                            contextMenu.popup()
                    }
                    onDoubleClicked: (mouse) => {
                        if (mouse.modifiers !== Qt.NoModifier)
                            return
                        selection.selectOnly(delegateRoot.index)
                        listView.currentIndex = delegateRoot.index
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

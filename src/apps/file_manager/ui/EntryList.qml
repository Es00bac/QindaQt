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

    property int iconSize: 64
    readonly property int rowIconSize: Math.max(20, Math.round(iconSize * 0.4375))
    readonly property int rowHeight: rowIconSize + 16
    signal zoomRequested(int steps)

    ViewportNavigation {
        id: keyboardNavigation
        view: listView
        selection: root.selection
        navigationController: root.navigationController
        columns: 1
        rowHeight: root.rowHeight
        onContextMenuRequested: contextMenu.popup()
    }
    onIconSizeChanged: keyboardNavigation.scheduleReveal()

    readonly property alias focusItem: listView

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
            Layout.rightMargin: 16
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

        Item {
            id: viewport
            Layout.fillWidth: true
            Layout.fillHeight: true

            ListView {
                id: listView
                objectName: "entryListView"
                anchors.fill: parent
                anchors.rightMargin: 16
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                onWidthChanged: keyboardNavigation.scheduleReveal()
                onHeightChanged: keyboardNavigation.scheduleReveal()
                onVisibleChanged: if (visible) keyboardNavigation.scheduleReveal()
                focus: true
                keyNavigationEnabled: false
                currentIndex: root.selection.currentIndex
                function selectEntry(index) { root.selection.selectOnly(index) }
                model: root.navigationController.entries

                T.ScrollBar.vertical: ViewportScrollBar {
                    objectName: "entryListScrollBar"
                    parent: viewport
                    x: listView.width + 4
                    height: listView.height
                }
                ZoomWheelHandler { onZoomRequested: (steps) => root.zoomRequested(steps) }

                Accessible.role: Accessible.List
                Accessible.name: qsTr("Folder contents")

                Keys.onReturnPressed: root.activateCurrent()
                Keys.onEnterPressed: root.activateCurrent()
                Keys.onPressed: (event) => keyboardNavigation.handle(event)

                delegate: Rectangle {
                    id: delegateRoot

                    required property var modelData
                    required property int index

                    property bool entrySelected: selection.isSelected(delegateRoot.index)

                    width: listView.width
                    height: root.rowHeight
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
                            Layout.preferredWidth: root.rowIconSize
                            Layout.preferredHeight: root.rowIconSize
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

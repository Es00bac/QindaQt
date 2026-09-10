// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "EntryDrag.js" as EntryDrag

// Visual browsing shares the exact selection policy with the details view.
Control {
    id: root

    required property var navigationController
    required property var selection
    required property var appCoordinator
    // Optional drop dispatch targets; Main always passes the real controllers,
    // fixture tests may leave them null (drops then refuse politely).
    property var mutationController: null
    property var clipboardController: null

    property int iconSize: 64
    signal zoomRequested(int steps)

    ViewportNavigation {
        id: keyboardNavigation
        view: gridView
        selection: root.selection
        navigationController: root.navigationController
        columns: Math.max(1, Math.floor(gridView.width / gridView.cellWidth))
        rowHeight: gridView.cellHeight
        onContextMenuRequested: contextMenu.popup()
    }
    onIconSizeChanged: keyboardNavigation.scheduleReveal()

    readonly property alias focusItem: gridView

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

    padding: 8

    contentItem: ColumnLayout {
        spacing: 0

        Item {
            id: viewport
            Layout.fillWidth: true
            Layout.fillHeight: true

            // Background drop target: dropping on empty view space drops into
            // the browsed folder. Folder delegates carry their own DropArea,
            // which sits above this one and wins when both cover the cursor.
            DropArea {
                anchors.fill: parent
                onEntered: (drag) => drag.accepted = EntryDrag.canAccept(drag)
                onDropped: (drop) => {
                    const action = EntryDrag.dispatch(
                        drop, root.navigationController.currentPath,
                        root.mutationController, root.clipboardController)
                    if (action !== Qt.IgnoreAction)
                        drop.accept(action)
                    else
                        drop.accepted = false
                }
            }

            GridView {
                id: gridView
                objectName: "entryGridView"
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
                cellWidth: Math.max(1, width / Math.max(1, Math.floor(width / (root.iconSize + 72))))
                cellHeight: root.iconSize + 80
                model: root.navigationController.entries

                ScrollBar.vertical: ViewportScrollBar {
                    objectName: "entryGridScrollBar"
                    parent: viewport
                    x: gridView.width + 4
                    height: gridView.height
                }
                ZoomWheelHandler { onZoomRequested: (steps) => root.zoomRequested(steps) }

                Accessible.role: Accessible.List
                Accessible.name: qsTr("Folder contents")

                Keys.onReturnPressed: { root.activateCurrent(); }
                Keys.onEnterPressed: { root.activateCurrent(); }
                Keys.onPressed: (event) => { keyboardNavigation.handle(event); }

                delegate: Rectangle {
                    id: delegateRoot

                    required property var modelData
                    required property int index

                    property bool entrySelected: selection.isSelected(delegateRoot.index)

                    width: gridView.cellWidth - 4
                    height: gridView.cellHeight - 4
                    radius: 8
                    color: delegateRoot.entrySelected ? root.palette.highlight
                         : hoverArea.containsMouse ? root.palette.alternateBase : "transparent"

                    Drag.active: dragHandler.active
                    Drag.dragType: Drag.Automatic
                    Drag.supportedActions: Qt.CopyAction | Qt.MoveAction
                    Drag.mimeData: EntryDrag.mimeFor(
                        delegateRoot.entrySelected ? root.selection.selectedEntries()
                                                   : [delegateRoot.modelData])

                    DragHandler {
                        id: dragHandler
                        target: null
                    }

                    Accessible.role: Accessible.ListItem
                    Accessible.name: delegateRoot.modelData.name + (delegateRoot.modelData.isDirectory
                        ? qsTr(", folder") : qsTr(", file"))
                    Accessible.selected: delegateRoot.entrySelected
                    border.width: GridView.isCurrentItem && gridView.activeFocus ? 2 : 0
                    border.color: root.palette.highlight

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 4

                        Item {
                            Layout.alignment: Qt.AlignHCenter
                            Layout.preferredWidth: root.iconSize + 16
                            Layout.preferredHeight: root.iconSize + 16
                            Image {
                                anchors.centerIn: parent
                                width: root.iconSize
                                height: root.iconSize
                                sourceSize: Qt.size(root.iconSize, root.iconSize)
                                source: "image://theme-icons/" + (delegateRoot.modelData.iconName || "application-octet-stream")
                                visible: previewImage.status !== Image.Ready || previewImage.implicitWidth <= 1
                                Accessible.ignored: true
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
                            Image {
                                visible: delegateRoot.modelData.isSymlink
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                width: 20
                                height: 20
                                sourceSize: Qt.size(20, 20)
                                source: "image://theme-icons/emblem-symbolic-link"
                                Accessible.ignored: true
                            }
                            Accessible.ignored: true
                        }

                        Label {
                            Layout.fillWidth: true
                            horizontalAlignment: Text.AlignHCenter
                            text: delegateRoot.modelData.name
                            color: delegateRoot.entrySelected ? root.palette.highlightedText
                                 : delegateRoot.modelData.isHidden ? root.palette.placeholderText
                                 : root.palette.text
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

                    DropArea {
                        anchors.fill: parent
                        enabled: delegateRoot.modelData.isDirectory
                        onEntered: (drag) => drag.accepted = EntryDrag.canAccept(drag)
                        onDropped: (drop) => {
                            const action = EntryDrag.dispatch(
                                drop, delegateRoot.modelData.path,
                                root.mutationController, root.clipboardController)
                            if (action !== Qt.IgnoreAction)
                                drop.accept(action)
                            else
                                drop.accepted = false
                        }
                    }
                }
            }

        }

        Menu {
            id: contextMenu

            MenuItem {
                text: qsTr("Cut")
                onTriggered: root.appCoordinator.activateAction("edit.cut")
            }
            MenuItem {
                text: qsTr("Copy")
                onTriggered: root.appCoordinator.activateAction("edit.copy")
            }
            MenuItem {
                text: qsTr("Paste")
                onTriggered: root.appCoordinator.activateAction("edit.paste")
            }
            MenuItem {
                text: qsTr("Rename")
                onTriggered: root.appCoordinator.activateAction("file.rename")
            }
            MenuItem {
                text: qsTr("Copy To…")
                onTriggered: root.appCoordinator.activateAction("file.copy")
            }
            MenuItem {
                text: qsTr("Move To…")
                onTriggered: root.appCoordinator.activateAction("file.move")
            }
            MenuItem {
                text: qsTr("Move to Trash")
                onTriggered: root.appCoordinator.activateAction("file.trash")
            }
            MenuItem {
                text: qsTr("Properties")
                onTriggered: root.appCoordinator.activateAction("file.properties")
            }
        }

        Label {
            Layout.fillWidth: true
            Layout.topMargin: 4
            visible: root.navigationController.statusMessage.length > 0
            text: root.navigationController.statusMessage
            color: root.palette.placeholderText
        }
    }
}

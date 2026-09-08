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

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Tokens.space["2"]
        spacing: 0

        Item {
            id: viewport
            Layout.fillWidth: true
            Layout.fillHeight: true

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

                T.ScrollBar.vertical: ViewportScrollBar {
                    objectName: "entryGridScrollBar"
                    parent: viewport
                    x: gridView.width + 4
                    height: gridView.height
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
                            Layout.preferredWidth: root.iconSize + 16
                            Layout.preferredHeight: root.iconSize + 16
                            Qinda.Icon {
                                anchors.centerIn: parent
                                width: root.iconSize
                                height: root.iconSize
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

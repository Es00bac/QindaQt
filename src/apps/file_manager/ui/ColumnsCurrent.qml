// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QindaTK as Tk
import "EntryDrag.js" as EntryDrag
import "EntryText.js" as EntryText
import QindaQt.Controls 1.0 as C

// The Columns view's browsed column (ADR-0270): NavigationController's
// listing with the window's selection, rubber band, drag and drop, keyboard
// and context menu, as in every view. Left and Right go up and down a folder
// level (ColumnsView.goOut/goIn); every other key is the shared navigation.
// Extracted from ColumnsView so both stay within the source-shape budget.
Item {
    id: root

    required property var view
    readonly property alias listView: list

    DropArea {
        anchors.fill: parent
        onEntered: (drag) => drag.accepted = EntryDrag.canAccept(drag)
        onDropped: (drop) => {
            const action = EntryDrag.dispatch(drop, root.view.navigationController.currentPath,
                                              root.view.mutationController,
                                              root.view.clipboardController)
            if (action !== Qt.IgnoreAction)
                drop.accept(action)
            else
                drop.accepted = false
        }
    }
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.RightButton
        onClicked: {
            root.view.focusView()
            root.view.popupFor(0)
        }
    }
    C.TouchContextArea {
        objectName: "entryColumnsTouchContext"
        anchors.fill: parent
        onContextRequested: {
            root.view.focusView()
            root.view.popupFor(0)
        }
    }

    ListView {
        id: list
        objectName: "entryColumnsView"
        anchors.fill: parent
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        focus: true
        keyNavigationEnabled: false
        model: root.view.selection.entries
        currentIndex: root.view.selection.currentIndex
        function selectEntry(index) { root.view.selection.selectOnly(index) }

        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
        ZoomWheelHandler { onZoomRequested: (steps) => root.view.zoomRequested(steps) }
        SelectionBand {
            anchors.fill: parent
            view: list
            onFinished: (indexes, modifiers) => root.view.selection.applyIndexSet(indexes, modifiers)
        }

        Accessible.role: Accessible.List
        Accessible.name: qsTr("Folder contents")
        Keys.onReturnPressed: root.view.activateCurrent()
        Keys.onEnterPressed: root.view.activateCurrent()
        Keys.onPressed: (event) => {
            const plain = event.modifiers === Qt.NoModifier
            if (plain && event.key === Qt.Key_Left) {
                root.view.goOut()
                event.accepted = true
            } else if (plain && event.key === Qt.Key_Right) {
                root.view.goIn()
                event.accepted = true
            } else {
                root.view.handleKey(event)
            }
        }

        delegate: Rectangle {
            id: row
            required property var modelData
            required property int index
            readonly property bool entrySelected: root.view.selection.isSelected(row.index)

            width: ListView.view ? ListView.view.width : 0
            height: root.view.rowHeight
            color: row.entrySelected ? Tk.Theme.color.selection
                 : rowMouse.containsMouse ? Tk.Theme.color.hover : "transparent"
            border.width: row.ListView.isCurrentItem && list.activeFocus ? Tk.Theme.size.focusRing : 0
            border.color: Tk.Theme.color.focus

            Drag.active: dragHandler.active
            Drag.dragType: Drag.Automatic
            Drag.supportedActions: Qt.CopyAction | Qt.MoveAction
            Drag.mimeData: EntryDrag.mimeFor(row.entrySelected ? root.view.selection.selectedEntries()
                                                               : [row.modelData])
            DragHandler {
                id: dragHandler
                target: null
            }

            Accessible.role: Accessible.ListItem
            Accessible.name: row.modelData.name + EntryText.kindSuffix(row.modelData)
            Accessible.description: row.modelData.note || ""
            Accessible.selected: row.entrySelected

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Tk.Theme.space.md
                anchors.rightMargin: Tk.Theme.space.sm
                spacing: Tk.Theme.space.md
                opacity: row.modelData.isHidden === true || row.modelData.launchable === false
                    ? Tk.Theme.opacity.muted : 1
                Image {
                    Layout.preferredWidth: root.view.rowIconSize
                    Layout.preferredHeight: root.view.rowIconSize
                    sourceSize: Qt.size(root.view.rowIconSize, root.view.rowIconSize)
                    source: EntryText.iconUrl(row.modelData, root.view.rowIconSize)
                    Accessible.ignored: true
                }
                Tk.Label {
                    Layout.fillWidth: true
                    text: EntryText.displayName(row.modelData, root.view.showExtensions)
                    elide: Text.ElideMiddle
                    Accessible.ignored: true
                }
                Tk.Icon {
                    visible: row.modelData.isDirectory === true
                    name: "chevron-right"
                    size: Tk.Theme.size.iconSm
                    color: Tk.Theme.color.textMuted
                }
            }

            MouseArea {
                id: rowMouse
                anchors.fill: parent
                hoverEnabled: true
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                ToolTip.visible: containsMouse && (row.modelData.note || "").length > 0
                ToolTip.text: row.modelData.note || ""
                ToolTip.delay: 600
                onClicked: (mouse) => {
                    list.forceActiveFocus()
                    root.view.selection.click(row.index, mouse.button, mouse.modifiers)
                    if (mouse.button === Qt.RightButton)
                        root.view.popupFor(root.view.selection.selectedEntries().length)
                }
                onDoubleClicked: (mouse) => {
                    if (mouse.modifiers !== Qt.NoModifier)
                        return
                    root.view.selection.selectOnly(row.index)
                    root.view.activateCurrent()
                }
            }
            C.TouchContextArea {
                objectName: "entryTouchContext"
                anchors.fill: parent
                onContextRequested: {
                    list.forceActiveFocus()
                    root.view.selection.target(row.index)
                    root.view.popupFor(root.view.selection.selectedEntries().length)
                }
            }
            DropArea {
                anchors.fill: parent
                enabled: row.modelData.isDirectory === true
                onEntered: (drag) => drag.accepted = EntryDrag.canAccept(drag)
                onDropped: (drop) => {
                    const action = EntryDrag.dispatch(drop, row.modelData.path,
                                                      root.view.mutationController,
                                                      root.view.clipboardController)
                    if (action !== Qt.IgnoreAction)
                        drop.accept(action)
                    else
                        drop.accepted = false
                }
            }
        }
    }
}

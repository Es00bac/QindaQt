// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "EntryDrag.js" as EntryDrag

// One Details-mode row. Extracted from EntryList.qml so both files stay under
// the source-shape budget; the owning view injects everything the row needs.
Rectangle {
    id: delegateRoot

    required property var modelData
    required property int index
    required property var selection
    required property var viewPalette
    required property int rowHeight
    required property int rowIconSize
    required property real viewWidth
    // Optional drop dispatch targets; Main always passes the real controllers,
    // fixture tests may leave them null (drops then refuse politely).
    property var mutationController: null
    property var clipboardController: null

    signal activated()
    signal contextMenuRequested()

    property bool entrySelected: selection.isSelected(delegateRoot.index)

    width: ListView.view ? ListView.view.width : 0
    height: delegateRoot.rowHeight
    radius: 8
    color: delegateRoot.entrySelected ? delegateRoot.viewPalette.highlight
         : hoverArea.containsMouse ? delegateRoot.viewPalette.alternateBase : "transparent"

    Drag.active: dragHandler.active
    Drag.dragType: Drag.Automatic
    Drag.supportedActions: Qt.CopyAction | Qt.MoveAction
    Drag.mimeData: EntryDrag.mimeFor(
        delegateRoot.entrySelected ? delegateRoot.selection.selectedEntries()
                                   : [delegateRoot.modelData])

    DragHandler {
        id: dragHandler
        target: null
    }

    Accessible.role: Accessible.ListItem
    Accessible.name: delegateRoot.modelData.name + (delegateRoot.modelData.isDirectory
        ? qsTr(", folder") : qsTr(", file"))
    Accessible.selected: delegateRoot.entrySelected
    border.width: ListView.isCurrentItem && ListView.view && ListView.view.activeFocus ? 2 : 0
    border.color: delegateRoot.viewPalette.highlight

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        spacing: 8

        Image {
            Layout.preferredWidth: delegateRoot.rowIconSize
            Layout.preferredHeight: delegateRoot.rowIconSize
            sourceSize: Qt.size(delegateRoot.rowIconSize, delegateRoot.rowIconSize)
            source: "image://theme-icons/" + (delegateRoot.modelData.iconName || "application-octet-stream")
            Accessible.ignored: true
        }
        Label {
            Layout.fillWidth: true
            text: delegateRoot.modelData.name
            color: delegateRoot.entrySelected ? delegateRoot.viewPalette.highlightedText
                 : delegateRoot.modelData.isHidden ? delegateRoot.viewPalette.placeholderText
                 : delegateRoot.viewPalette.text
            elide: Text.ElideMiddle
            Accessible.ignored: true
        }
        Label {
            Layout.preferredWidth: 102
            horizontalAlignment: Text.AlignRight
            text: delegateRoot.modelData.sizeText
            color: delegateRoot.entrySelected ? delegateRoot.viewPalette.highlightedText : delegateRoot.viewPalette.placeholderText
            elide: Text.ElideRight
            Accessible.ignored: true
        }
        Label {
            Layout.preferredWidth: 102
            visible: delegateRoot.viewWidth > 580
            text: delegateRoot.modelData.kindText
            color: delegateRoot.entrySelected ? delegateRoot.viewPalette.highlightedText : delegateRoot.viewPalette.placeholderText
            elide: Text.ElideRight
            Accessible.ignored: true
        }
        Label {
            Layout.preferredWidth: 102
            visible: delegateRoot.viewWidth > 440
            text: delegateRoot.modelData.modifiedText
            color: delegateRoot.entrySelected ? delegateRoot.viewPalette.highlightedText : delegateRoot.viewPalette.placeholderText
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
            if (delegateRoot.ListView.view)
                delegateRoot.ListView.view.forceActiveFocus();
            if (mouse.button === Qt.RightButton && delegateRoot.selection.isSelected(delegateRoot.index)) {
            } else if (mouse.modifiers & Qt.ControlModifier) {
                delegateRoot.selection.toggle(delegateRoot.index);
            } else if (mouse.modifiers & Qt.ShiftModifier) {
                delegateRoot.selection.rangeTo(delegateRoot.index);
            } else {
                delegateRoot.selection.selectOnly(delegateRoot.index);
            }
            delegateRoot.selection.focusIndex(delegateRoot.index);
            if (mouse.button === Qt.RightButton)
                delegateRoot.contextMenuRequested();
        }
        onDoubleClicked: (mouse) => {
            if (mouse.modifiers !== Qt.NoModifier)
                return;
            delegateRoot.selection.selectOnly(delegateRoot.index);
            delegateRoot.selection.focusIndex(delegateRoot.index);
            delegateRoot.activated();
        }
    }

    DropArea {
        anchors.fill: parent
        enabled: delegateRoot.modelData.isDirectory
        onEntered: (drag) => { drag.accepted = EntryDrag.canAccept(drag); }
        onDropped: (drop) => {
            const action = EntryDrag.dispatch(
                drop, delegateRoot.modelData.path,
                delegateRoot.mutationController, delegateRoot.clipboardController);
            if (action !== Qt.IgnoreAction)
                drop.accept(action);
            else
                drop.accepted = false;
        }
    }
}

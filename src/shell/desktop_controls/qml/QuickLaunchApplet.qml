// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0 as C
import QindaQt.Shell.Icons 1.0 as ShellIcons
import QindaQt.Tokens 1.0

// Pinned-application strip: the launcher's pinned entries as icon buttons.
// Activation, unpinning, and reordering all re-enter the launcher facade.
Item {
    id: root

    required property var access
    property bool vertical: false

    readonly property bool ready: access !== null && Tokens.ready
    readonly property var rows: ready ? access.rows : []
    readonly property bool showRows: ready && rows.length > 0
    readonly property int iconExtent: Math.max(0, Math.min(20, (vertical ? width : height) - Tokens.space["2"]))
    property string contextEntryId: ""

    objectName: "quickLaunchApplet"
    implicitWidth: showRows ? strip.implicitWidth : placeholder.implicitWidth + Tokens.space["2"]
    implicitHeight: showRows ? strip.implicitHeight : 28

    Accessible.role: Accessible.Grouping
    Accessible.name: qsTr("Quick launch")
    Accessible.description: !ready ? qsTr("Quick launch is not connected")
                            : rows.length === 0 ? qsTr("No pinned applications")
                            : qsTr("%1 pinned applications").arg(rows.length)

    function focusIndex(index) {
        if (index < 0 || index >= repeater.count)
            return
        const item = repeater.itemAt(index)
        if (item !== null)
            item.forceActiveFocus(Qt.TabFocusReason)
    }

    ShellIcons.Icon {
        id: placeholder
        objectName: "quickLaunchPlaceholder"
        visible: !root.showRows
        anchors.centerIn: parent
        name: "applications-other"
        size: 18
        color: Tokens.fg.disabled
        symbolic: true
        fallbackText: qsTr("Quick launch")
        Accessible.ignored: true
    }

    GridLayout {
        id: strip
        anchors.fill: parent
        visible: root.showRows
        flow: root.vertical ? GridLayout.TopToBottom : GridLayout.LeftToRight
        rows: root.vertical ? -1 : 1
        columns: root.vertical ? 1 : -1
        rowSpacing: Tokens.space["1"]
        columnSpacing: Tokens.space["1"]

        Repeater {
            id: repeater
            model: root.rows

            T.Button {
                id: entryButton

                required property var modelData
                required property int index

                objectName: "quickLaunchEntry"
                Layout.preferredWidth: root.iconExtent + Tokens.space["2"] * 2
                Layout.preferredHeight: root.iconExtent + Tokens.space["2"] * 2
                padding: Tokens.space["2"]
                focusPolicy: Qt.TabFocus
                hoverEnabled: true
                enabled: root.ready && Boolean(root.access.launchGranted)

                Accessible.role: Accessible.Button
                Accessible.name: String(modelData.accessibleName)
                Accessible.description: String(modelData.accessibleDescription)

                function activate() { root.access.activate(String(modelData.entryId)) }
                function openContext() {
                    root.contextEntryId = String(modelData.entryId)
                    contextMenu.popup(entryButton)
                }

                onClicked: activate()
                Keys.onReturnPressed: activate()
                Keys.onEnterPressed: activate()
                Accessible.onPressAction: activate()
                Keys.onLeftPressed: if (!root.vertical) root.focusIndex(index - 1)
                Keys.onRightPressed: if (!root.vertical) root.focusIndex(index + 1)
                Keys.onUpPressed: if (root.vertical) root.focusIndex(index - 1)
                Keys.onDownPressed: if (root.vertical) root.focusIndex(index + 1)
                Keys.onPressed: (event) => {
                    if (event.key === Qt.Key_Menu
                            || (event.key === Qt.Key_F10 && (event.modifiers & Qt.ShiftModifier))) {
                        openContext()
                        event.accepted = true
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.RightButton
                    onClicked: entryButton.openContext()
                }

                contentItem: ShellIcons.Icon {
                    objectName: "quickLaunchEntryIcon"
                    name: String(entryButton.modelData.iconName)
                    size: root.iconExtent
                    color: entryButton.enabled ? Tokens.fg.default : Tokens.fg.disabled
                    symbolic: false
                    fallbackText: String(entryButton.modelData.displayText)
                    Accessible.ignored: true
                }

                background: Rectangle {
                    radius: Tokens.radius.m
                    color: entryButton.down ? Tokens.state.pressed
                         : entryButton.hovered ? Tokens.state.hover : "transparent"
                    C.FocusRing { anchors.fill: parent; control: entryButton }
                }
            }
        }
    }

    // QindaQt.Controls ships no menu primitive yet (task-list precedent); the
    // context menu uses the QQC2 style palette and owns no applet colors.
    T.Menu {
        id: contextMenu
        objectName: "quickLaunchContextMenu"
        popupType: T.Popup.Window

        T.MenuItem {
            objectName: "quickLaunchMoveUp"
            text: root.vertical ? qsTr("Move up") : qsTr("Move left")
            onTriggered: root.access.moveUp(root.contextEntryId)
        }
        T.MenuItem {
            objectName: "quickLaunchMoveDown"
            text: root.vertical ? qsTr("Move down") : qsTr("Move right")
            onTriggered: root.access.moveDown(root.contextEntryId)
        }
        T.MenuItem {
            objectName: "quickLaunchUnpin"
            text: qsTr("Unpin")
            onTriggered: root.access.unpin(root.contextEntryId)
        }
    }

    ControlPopupFrame {
        id: feedbackPopup
        objectName: "quickLaunchFeedbackPopup"
        visible: root.ready && Boolean(root.access.feedbackPresent)
        heading: qsTr("Quick launch")
        feedback: root.ready ? String(root.access.feedback) : ""
        initialFocusItem: dismiss

        C.Button {
            id: dismiss
            objectName: "quickLaunchFeedbackDismiss"
            Layout.alignment: Qt.AlignRight
            text: qsTr("Dismiss")
            emphasized: false
            onClicked: root.access.clearFeedback()
        }
    }
}

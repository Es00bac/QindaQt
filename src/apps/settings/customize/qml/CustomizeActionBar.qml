// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

ColumnLayout {
    id: root

    required property var customizeSettings
    readonly property Item firstFocusTarget: undoButton
    signal closeRequested()
    signal closeCancelled()

    function requestClose() {
        if (root.customizeSettings.dirty) {
            discardDialog.open()
        } else {
            root.closeRequested()
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Tokens.space["2"]

        Button {
            id: undoButton
            objectName: "customizeUndoButton"
            text: qsTr("Undo")
            emphasized: false
            available: root.customizeSettings.canUndo
            onClicked: root.customizeSettings.undo()
        }
        Button {
            objectName: "customizeRedoButton"
            text: qsTr("Redo")
            emphasized: false
            available: root.customizeSettings.canRedo
            onClicked: root.customizeSettings.redo()
        }
        Item { Layout.fillWidth: true }
        Button {
            objectName: "customizeDiscardButton"
            text: qsTr("Discard")
            emphasized: false
            available: root.customizeSettings.canEdit
                       && root.customizeSettings.dirty
            onClicked: discardDialog.open()
        }
        Button {
            objectName: "customizeApplyButton"
            text: qsTr("Apply")
            available: root.customizeSettings.applyAvailable
            busy: root.customizeSettings.saving
            onClicked: root.customizeSettings.apply()
        }
        Button {
            objectName: "customizeCloseButton"
            text: qsTr("Close")
            emphasized: false
            available: !root.customizeSettings.saving
            onClicked: root.requestClose()
        }
    }

    T.Dialog {
        id: discardDialog
        objectName: "customizeDiscardDialog"
        title: qsTr("Discard layout changes?")
        modal: true
        width: Math.min(480, root.width - 2 * Tokens.space["4"])
        anchors.centerIn: Overlay.overlay
        standardButtons: T.Dialog.Discard | T.Dialog.Cancel
        contentItem: Label {
            width: 360
            text: qsTr("The current in-memory layout has not been applied.")
            wrapMode: Text.Wrap
            Accessible.role: Accessible.StaticText
            Accessible.name: text
        }
        onDiscarded: {
            root.customizeSettings.discard()
            if (!root.customizeSettings.dirty) {
                root.closeRequested()
            }
        }
        onRejected: root.closeCancelled()
    }
}

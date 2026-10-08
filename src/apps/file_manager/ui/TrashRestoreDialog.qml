// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root
    required property var controller
    property var items: []
    property string originalPath: ""
    property string initialFolder: ""
    property string refusal: ""
    objectName: "trashRestoreDialog"
    title: qsTr("Restore from Trash")
    modal: true
    width: Math.min(600, parent ? parent.width - 32 : 600)
    onOpened: { folder.text = root.initialFolder; root.refusal = "" }

    function restore(chosen) {
        if (root.controller.busy) {
            root.refusal = qsTr("Wait for the current file operation to finish.")
            return
        }
        const admitted = root.items.length > 0
            ? (chosen ? root.controller.putBackItemsTo(root.items, folder.text)
                      : root.controller.putBackItems(root.items))
            : (chosen ? root.controller.restoreLastTo(folder.text)
                      : root.controller.restoreLast())
        if (admitted) root.close()
        else root.refusal = root.controller.failureMessage
    }
    contentItem: ColumnLayout {
        spacing: 12
        Label {
            Layout.fillWidth: true
            text: qsTr("Restore to the original folder, or deliberately choose an existing local folder. A name collision will keep the payload in Trash.")
            textFormat: Text.PlainText
            wrapMode: Text.Wrap
            Accessible.name: text
        }
        Label {
            Layout.fillWidth: true
            text: root.originalPath
            textFormat: Text.PlainText
            wrapMode: Text.WrapAnywhere
            Accessible.name: qsTr("Original path: %1").arg(text)
        }
        TextField {
            id: folder
            objectName: "trashRestoreFolder"
            Layout.fillWidth: true
            enabled: !root.controller.busy
            placeholderText: qsTr("Existing restore folder")
            Accessible.name: qsTr("Existing local restore folder")
            onAccepted: root.restore(true)
        }
        Label {
            Layout.fillWidth: true
            visible: root.refusal.length > 0
            text: root.refusal
            textFormat: Text.PlainText
            wrapMode: Text.Wrap
            Accessible.name: text
        }
        RowLayout {
            Button {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                contentItem: Label { text: parent.text; textFormat: Text.PlainText; wrapMode: Text.Wrap }
                objectName: "trashRestoreOriginal"
                text: qsTr("Original folder")
                enabled: !root.controller.busy
                onClicked: root.restore(false)
            }
            Button {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                contentItem: Label { text: parent.text; textFormat: Text.PlainText; wrapMode: Text.Wrap }
                objectName: "trashRestoreChosen"
                text: qsTr("Chosen folder")
                enabled: !root.controller.busy && folder.text.length > 0
                onClicked: root.restore(true)
            }
            Button {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                contentItem: Label { text: parent.text; textFormat: Text.PlainText; wrapMode: Text.Wrap }
                text: qsTr("Cancel"); onClicked: root.close()
            }
        }
    }
}

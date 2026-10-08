// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
Dialog {
    id: root
    objectName: "moveRecoveryRestoreConfirmation"
    property var receipt: null
    signal restoreRequested(string operationId)
    parent: Overlay.overlay
    anchors.centerIn: parent
    modal: true
    width: Math.min(560, parent ? parent.width - 32 : 560)
    title: qsTr("Restore current retained source?")
    standardButtons: Dialog.Ok | Dialog.Cancel
    onAccepted: {
        if (receipt)
            restoreRequested(receipt.operationId)
    }
    Label {
        width: parent.width
        text: root.receipt
            ? qsTr("Restore current retained contents to:\n%1\nAn existing item will never be replaced. The published destination stays in place.")
                .arg(root.receipt.sourcePath) : ""
        textFormat: Text.PlainText
        wrapMode: Text.WordWrap
        Accessible.name: text
    }
}

// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QindaQt.Controls 1.0 as C
import QindaQt.Tokens 1.0

// The pairing request block of the QindaLink popup: the phone's sanitized
// name and model, the code to compare, and Reject/Accept.
// AGENT-GUARD: name and model are phone-supplied; they are rendered with
// Text.PlainText only, and Accept is never the default focus (the popup
// focuses `rejectButton`).
ColumnLayout {
    id: root

    required property var access
    readonly property alias rejectButton: rejectButton
    signal decided()

    spacing: 6

    Label {
        objectName: "linkAppletPairingPhone"
        Layout.fillWidth: true
        textFormat: Text.PlainText
        text: root.access !== null
            ? (root.access.pairingModel.length > 0
               ? qsTr("%1 (%2)").arg(root.access.pairingName).arg(root.access.pairingModel)
               : root.access.pairingName)
            : ""
        color: Tokens.fg.default
        wrapMode: Text.Wrap
    }
    Label {
        Layout.fillWidth: true
        textFormat: Text.PlainText
        text: qsTr("Make sure this code matches the one on your phone.")
        color: Tokens.fg.muted
        wrapMode: Text.Wrap
    }
    Label {
        objectName: "linkAppletPairingCode"
        Layout.alignment: Qt.AlignHCenter
        textFormat: Text.PlainText
        text: root.access !== null ? root.access.pairingCode : ""
        font.bold: true
        font.pointSize: Tokens.type.title
        font.letterSpacing: 2
        color: Tokens.fg.default
        Accessible.name: qsTr("Pairing code %1").arg(text)
    }
    RowLayout {
        Layout.fillWidth: true
        spacing: 6
        C.Button {
            id: rejectButton
            objectName: "linkAppletRejectButton"
            text: qsTr("Reject")
            emphasized: false
            enabled: root.access !== null && root.access.canControl
            accessibleDescription: qsTr("Refuse to pair with this phone")
            onClicked: {
                root.access.rejectPairing()
                root.decided()
            }
        }
        C.Button {
            id: acceptButton
            objectName: "linkAppletAcceptButton"
            text: qsTr("Accept")
            emphasized: false
            enabled: root.access !== null && root.access.canControl
            accessibleDescription: qsTr("Pair with this phone; the code must match")
            onClicked: {
                root.access.acceptPairing()
                root.decided()
            }
        }
    }
}

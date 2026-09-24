// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QindaQt.Controls 1.0 as C

// One visible Wi-Fi network. Saved and secured are spelled out in words so
// neither depends on colour or a glyph alone.
Frame {
    id: root

    required property var row
    required property var access
    required property var colors

    readonly property string details: {
        const facts = [qsTr("Signal %1%").arg(row.signalPercent)]
        if (row.active)
            facts.push(qsTr("Connected"))
        if (row.saved)
            facts.push(qsTr("Saved"))
        facts.push(row.secured ? qsTr("Secured (%1)").arg(row.securityLabel)
                               : qsTr("Open"))
        if (row.pending)
            facts.push(qsTr("connecting…"))
        return facts.join(" · ")
    }

    objectName: "networkAppletAccessPointRow"

    RowLayout {
        anchors.fill: parent
        spacing: 8

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            Label {
                Layout.fillWidth: true
                text: root.row.label
                color: root.colors.text ?? "white"
                font.bold: root.row.active
                elide: Text.ElideRight
                Accessible.name: root.row.accessibleName
                Accessible.description: root.row.accessibleDescription
            }
            Label {
                objectName: "networkAppletAccessPointDetails"
                Layout.fillWidth: true
                text: root.details
                color: root.colors.textMuted ?? "#a9afa9"
                wrapMode: Text.Wrap
            }
        }

        C.Button {
            objectName: "networkAppletConnectButton"
            visible: !root.row.active
            text: qsTr("Connect")
            emphasized: false
            available: root.row.canConnect && !root.row.pending
            busy: root.row.pending
            accessibleDescription: root.row.saved
                ? qsTr("Connect to the saved network %1").arg(root.row.label)
                : root.row.secured
                  ? qsTr("Join %1; a password prompt appears if one is needed").arg(root.row.label)
                  : qsTr("Join the open network %1").arg(root.row.label)
            onClicked: root.access.requestConnect(root.row.id)
        }
    }
}

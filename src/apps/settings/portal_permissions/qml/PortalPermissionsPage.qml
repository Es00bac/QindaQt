// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0
T.Page {
    id: root
    required property var permissions
    signal closeRequested()
    readonly property Item firstFocusTarget: refreshButton
    title: qsTr("Portal permissions")
    background: Rectangle { color: Tokens.bg.base }
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Tokens.space["5"]
        spacing: Tokens.space["3"]
        Label {
            text: qsTr("Portal permissions")
            font.pointSize: Tokens.type.title
            font.weight: Font.DemiBold
            Accessible.role: Accessible.Heading
            Accessible.name: text
        }
        Label {
            Layout.fillWidth: true
            text: qsTr("Remembered screen sharing and remote desktop grants. Revoking a grant makes the application ask again; it does not stop an active session.")
            wrapMode: Text.Wrap
        }
        Label {
            objectName: "portalPermissionsError"
            Layout.fillWidth: true
            visible: text.length > 0
            text: root.permissions.errorText
            textFormat: Text.PlainText
            wrapMode: Text.Wrap
            Accessible.role: Accessible.AlertMessage
            Accessible.name: text
        }
        Button {
            id: refreshButton
            objectName: "portalPermissionsRefresh"
            text: root.permissions.busy ? qsTr("Loading…") : qsTr("Refresh")
            enabled: !root.permissions.busy
            onClicked: root.permissions.refresh()
        }
        Label {
            objectName: "portalPermissionsEmpty"
            visible: root.permissions.available && !root.permissions.busy && list.count === 0
            text: qsTr("No remembered grants.")
        }
        ListView {
            id: list
            objectName: "portalPermissionsList"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: Tokens.space["2"]
            model: root.permissions.rows
            T.ScrollBar.vertical: T.ScrollBar {}
            delegate: RowLayout {
                id: row
                required property var modelData
                width: list.width
                Label {
                    Layout.fillWidth: true
                    text: row.modelData.app + " — " + row.modelData.family
                    textFormat: Text.PlainText
                    wrapMode: Text.Wrap
                    Accessible.name: text
                }
                Button {
                    objectName: "portalPermissionRevoke"
                    text: qsTr("Revoke")
                    enabled: root.permissions.available && !root.permissions.busy
                    Accessible.name: qsTr("Revoke %1 for %2").arg(row.modelData.family).arg(row.modelData.app)
                    onClicked: root.permissions.revoke(row.modelData.key)
                }
            }
        }
    }
}

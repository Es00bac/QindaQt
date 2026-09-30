// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

T.Page {
    id: root
    required property var keyringSettings
    signal closeRequested()
    readonly property Item firstFocusTarget: collectionLabel
    title: qsTr("Passwords & Keys")
    background: Rectangle { color: Tokens.bg.base }
    Component.onCompleted: keyringSettings.reload()
    Component.onDestruction: keyringSettings.deactivate()

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Tokens.space["5"]
        spacing: Tokens.space["3"]
        Label {
            objectName: "keyringPageHeading"
            Layout.fillWidth: true
            text: qsTr("Passwords & Keys")
            font.pointSize: Tokens.type.title
            font.weight: Font.DemiBold
            Accessible.role: Accessible.Heading
        }
        StateCard {
            objectName: "keyringAvailabilityState"
            Layout.fillWidth: true
            status: root.keyringSettings.available ? StateCard.Success : StateCard.Error
            title: root.keyringSettings.available ? qsTr("Key store connected") : qsTr("Key store unavailable")
            message: root.keyringSettings.status.length > 0 ? root.keyringSettings.status : qsTr("Collections stay locked until the service is available.")
            actionText: qsTr("Reload")
            onActionTriggered: root.keyringSettings.reload()
        }
        Label {
            objectName: "keyringPolicyStatus"
            Layout.fillWidth: true
            visible: text.length > 0
            text: root.keyringSettings.policyStatus
            wrapMode: Text.WordWrap
            Accessible.name: text
        }
        RowLayout {
            Layout.fillWidth: true
            T.TextField {
                id: collectionLabel
                objectName: "keyringNewCollectionLabel"
                Layout.fillWidth: true
                placeholderText: qsTr("New collection name")
                maximumLength: 128
                Accessible.name: qsTr("New collection name")
            }
            T.Button {
                objectName: "keyringCreateCollection"
                text: qsTr("Create")
                enabled: root.keyringSettings.available && !root.keyringSettings.busy && collectionLabel.text.trim().length > 0
                onClicked: { root.keyringSettings.createCollection(collectionLabel.text.trim()); collectionLabel.clear() }
            }
            T.Button {
                id: reloadButton
                objectName: "keyringReload"
                text: qsTr("Reload")
                enabled: !root.keyringSettings.busy
                onClicked: root.keyringSettings.reload()
            }
        }
        RowLayout {
            Layout.fillWidth: true
            T.ComboBox {
                id: collections
                objectName: "keyringCollections"
                Layout.fillWidth: true
                model: root.keyringSettings.collections
                textRole: "label"
                onActivated: index => root.keyringSettings.selectCollection(model[index].path)
                Accessible.name: qsTr("Collections")
            }
            T.Button { text: qsTr("Unlock"); enabled: root.keyringSettings.available && !root.keyringSettings.busy && root.keyringSettings.selectedCollectionPath.length > 0; onClicked: root.keyringSettings.unlockCollection() }
            T.Button { text: qsTr("Lock"); enabled: !root.keyringSettings.busy && root.keyringSettings.selectedCollectionPath.length > 0; onClicked: root.keyringSettings.lockCollection() }
            T.Button { text: qsTr("Change password"); enabled: root.keyringSettings.available && !root.keyringSettings.busy && root.keyringSettings.selectedCollectionPath.length > 0; onClicked: root.keyringSettings.changePassword() }
        }
        ListView {
            id: itemList
            objectName: "keyringItems"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: root.keyringSettings.items
            delegate: T.ItemDelegate {
                id: itemRow
                required property var modelData
                // Locked native metadata intentionally omits labels.
                readonly property bool rowAvailable: modelData !== undefined && modelData !== null
                readonly property string displayLabel: rowAvailable && !modelData.locked ? modelData.label : qsTr("Locked item")
                width: itemList.width
                text: itemRow.displayLabel
                Accessible.name: itemRow.displayLabel
                contentItem: RowLayout {
                    T.Label { Layout.fillWidth: true; text: itemRow.displayLabel; elide: Text.ElideRight }
                    T.Button { text: qsTr("Reveal"); enabled: itemRow.rowAvailable && root.keyringSettings.secretsAllowed && root.keyringSettings.available && !root.keyringSettings.busy && !itemRow.modelData.locked && itemRow.modelData.indexAuthenticated; onClicked: if (itemRow.rowAvailable) root.keyringSettings.revealItem(itemRow.modelData.path) }
                    T.Button { text: qsTr("Copy"); enabled: itemRow.rowAvailable && root.keyringSettings.secretsAllowed && root.keyringSettings.available && !root.keyringSettings.busy && !itemRow.modelData.locked && itemRow.modelData.indexAuthenticated; onClicked: if (itemRow.rowAvailable) root.keyringSettings.copyItem(itemRow.modelData.path) }
                    T.Button { text: qsTr("Delete"); enabled: itemRow.rowAvailable && root.keyringSettings.available && !root.keyringSettings.busy && !itemRow.modelData.locked && itemRow.modelData.indexAuthenticated; onClicked: if (itemRow.rowAvailable) root.keyringSettings.deleteItem(itemRow.modelData.path) }
                }
            }
            T.Label { anchors.centerIn: parent; visible: itemList.count === 0; text: qsTr("Select an unlocked collection to view its items."); wrapMode: Text.WordWrap }
        }
        RowLayout {
            Layout.fillWidth: true
            visible: root.keyringSettings.secretVisible
            T.Label { text: qsTr("Revealed secret") }
            T.Label {
                objectName: "keyringRevealedSecret"
                Layout.fillWidth: true
                text: root.keyringSettings.secretText
                wrapMode: Text.WrapAnywhere
                Accessible.name: qsTr("Temporarily revealed secret")
            }
            T.Button { text: qsTr("Hide"); onClicked: root.keyringSettings.clearSecret() }
        }
        Label {
            objectName: "keyringStatus"
            Layout.fillWidth: true
            visible: text.length > 0
            text: root.keyringSettings.status
            wrapMode: Text.WordWrap
        }
        RowLayout {
            Layout.fillWidth: true
            T.Switch {
                objectName: "keyringLockOnScreenLock"
                text: qsTr("Lock collections when the screen locks")
                checked: root.keyringSettings.preferences.lockOnScreenLock
                enabled: root.keyringSettings.preferences.available && !root.keyringSettings.preferences.busy
                onToggled: root.keyringSettings.preferences.setLockOnScreenLock(checked)
            }
            T.SpinBox {
                objectName: "keyringLockAfterIdleMinutes"
                from: 0; to: 1440; stepSize: 5
                value: root.keyringSettings.preferences.lockAfterIdleMinutes
                enabled: root.keyringSettings.preferences.available && !root.keyringSettings.preferences.busy
                onValueModified: root.keyringSettings.preferences.setLockAfterIdleMinutes(value)
                Accessible.name: qsTr("Lock after idle minutes; zero disables")
            }
        }
    }
}

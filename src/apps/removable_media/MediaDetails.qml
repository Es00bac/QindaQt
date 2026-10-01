// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Layouts
import QindaQt.Controls 1.0 as QQ
import QindaQt.Tokens 1.0

ColumnLayout {
    id: root
    required property var controller
    required property var volume
    readonly property string token: volume.token || ""
    readonly property bool hasVolume: token.length > 0
    readonly property bool usable: hasVolume && controller.available && !controller.busy
    readonly property var preferenceModes: ["ask", "mount", "read-only", "ignore"]
    spacing: Tokens.space["4"]
    visible: hasVolume
    onTokenChanged: passphrase.clear()

    QQ.SectionHeader {
        Layout.fillWidth: true
        title: root.volume.label || root.volume.device || qsTr("Media")
        description: (root.volume.device || "") + " · " + (root.volume.kind || "")
            + " · " + (root.volume.sizeText || "")
            + (root.volume.readOnly ? qsTr(" · Read-only media") : "")
    }
    QQ.Label {
        Layout.fillWidth: true
        visible: !!root.volume.mounted
        text: qsTr("Mounted at %1").arg(root.volume.mountPath || "")
        wrapMode: Text.WrapAnywhere
        muted: true
    }
    QQ.Label {
        Layout.fillWidth: true
        visible: !root.volume.mounted && !root.volume.mountable && !root.volume.encrypted
        text: root.volume.optical
            ? qsTr("This disc has no mountable data filesystem.")
            : root.volume.canFormat
                ? qsTr("No filesystem was found. Choose Format to prepare this media.")
                : qsTr("No mountable filesystem is available on this media.")
        wrapMode: Text.Wrap
        muted: true
    }

    Flow {
        Layout.fillWidth: true
        spacing: Tokens.space["2"]
        QQ.Button {
            objectName: "mountOpenButton"
            text: root.volume.mounted ? qsTr("Open in File Manager") : qsTr("Mount and open")
            available: root.usable && (!!root.volume.mounted || !!root.volume.mountable)
            onClicked: root.volume.mounted ? root.controller.open(root.token)
                                         : root.controller.mount(root.token, false, true)
        }
        QQ.Button {
            objectName: "mountReadOnlyButton"
            text: qsTr("Mount read-only")
            emphasized: false
            visible: !root.volume.mounted && !root.volume.encrypted
            available: root.usable && !!root.volume.canMountReadOnly
            onClicked: root.controller.mount(root.token, true, true)
        }
        QQ.Button {
            objectName: "unmountMediaButton"
            text: qsTr("Unmount")
            emphasized: false
            visible: !!root.volume.mounted
            available: root.usable
            onClicked: root.controller.unmount(root.token)
        }
        QQ.Button {
            objectName: "removeMediaButton"
            text: root.volume.optical && root.volume.canEject ? qsTr("Eject disc") : qsTr("Safely remove")
            emphasized: false
            visible: root.hasVolume
            available: root.usable
            accessibleDescription: qsTr("Finish using this media before disconnecting it.")
            onClicked: root.controller.remove(root.token)
        }
    }

    QQ.FormSurface {
        Layout.fillWidth: true
        visible: !!root.volume.encrypted
        padding: Tokens.space["4"]
        contentItem: ColumnLayout {
            id: unlockColumn
            QQ.Label { text: qsTr("Unlock this encrypted drive") }
            QQ.TextField {
                id: passphrase
                objectName: "mediaPassphrase"
                Layout.fillWidth: true
                enabled: root.usable
                echoMode: TextInput.Password
                placeholderText: qsTr("Passphrase")
                accessibleName: qsTr("Drive passphrase")
                onAccepted: if (unlockButton.enabled) unlockButton.click()
            }
            QQ.Button {
                id: unlockButton
                objectName: "unlockMediaButton"
                text: qsTr("Unlock")
                available: root.usable && passphrase.text.length > 0
                onClicked: {
                    root.controller.unlock(root.token, passphrase.text)
                    passphrase.clear()
                }
            }
        }
    }

    QQ.FormSurface {
        objectName: "insertionPreferenceSurface"
        Layout.fillWidth: true
        padding: Tokens.space["4"]
        contentItem: ColumnLayout {
            id: preferenceColumn
            QQ.Label { text: qsTr("When this media is inserted again") }
            QQ.ComboBox {
                objectName: "insertionPreference"
                Layout.fillWidth: true
                enabled: root.usable && !!root.volume.canRemember
                model: [qsTr("Ask me each time"), qsTr("Always mount"),
                        qsTr("Always mount read-only"), qsTr("Do nothing")]
                currentIndex: Math.max(0, root.preferenceModes.indexOf(root.volume.preference || "ask"))
                Accessible.name: qsTr("Action on future insertions")
                onActivated: root.controller.remember(root.token, root.preferenceModes[currentIndex])
            }
            QQ.Label {
                Layout.fillWidth: true
                text: root.volume.canRemember
                    ? qsTr("This choice applies only to this media. Formatting always needs confirmation.")
                    : qsTr("This media has no stable identity, so a future insertion choice cannot be saved.")
                muted: true
                wrapMode: Text.Wrap
            }
        }
    }

    QQ.Button {
        objectName: "formatMediaButton"
        text: qsTr("Format…")
        destructive: true
        visible: !!root.volume.canFormat
        available: root.usable
        accessibleDescription: qsTr("Open a separate confirmation before erasing this media.")
        onClicked: root.controller.requestFormat(root.token)
    }
}

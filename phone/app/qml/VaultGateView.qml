// SPDX-License-Identifier: LGPL-3.0-or-later
import QtQuick
import QindaTK as Tk

// Everything shown while the vault is not unlocked: create, unlock, waiting for the system prompt, and
// the recovery cases (key invalidated, integrity failure, no screen lock). `vault` is the context
// property (VaultBridge / FakeVault).
Tk.Flex {
    id: root

    property bool restoreOpen: false
    property bool eraseConfirm: false
    readonly property string st: vault.state

    direction: Tk.Flex.Column
    gap: Tk.Theme.space.md

    Tk.EmptyState {
        objectName: "gateState"
        visible: root.st === "none" || root.st === "locked" || root.st === "authenticating"
        iconName: root.st === "none" ? "key-round" : "lock"
        title: root.st === "none" ? qsTr("No vault yet")
            : root.st === "authenticating" ? qsTr("Waiting for you") : qsTr("Vault locked")
        text: root.st === "none"
            ? qsTr("Create a vault to keep passwords on this phone. It is protected by your fingerprint, face or screen lock.")
            : root.st === "authenticating" ? qsTr("Confirm with your fingerprint, face or screen lock.")
            : qsTr("Unlock with your fingerprint, face or screen lock.")
        Tk.Flex.alignSelf: Tk.Flex.Stretch
    }

    Tk.Notice {
        objectName: "gateNotice"
        visible: root.st === "nolock" || root.st === "invalidated" || root.st === "error" || root.st === "restore_ready"
            || (vault.message.length > 0 && (root.st === "none" || root.st === "locked"))
        variant: root.st === "restore_ready" ? "success" : root.st === "nolock" ? "warning"
            : (root.st === "none" || root.st === "locked") ? "info" : "danger"
        title: root.st === "invalidated" ? qsTr("Vault cannot be opened") : root.st === "error" ? qsTr("Vault not opened")
            : root.st === "nolock" ? qsTr("Screen lock required") : root.st === "restore_ready" ? qsTr("Backup ready") : ""
        text: root.st === "nolock" ? qsTr("Set a PIN, pattern, password or biometric in Android settings, then come back.")
            : root.st === "restore_ready"
                ? qsTr("%1 entries decrypted. Unlock to create a new vault from them.").arg(vault.restoreCount)
                : vault.message
        Tk.Flex.alignSelf: Tk.Flex.Stretch
    }

    Tk.Button {
        objectName: "gateUnlock"
        visible: root.st === "none" || root.st === "locked" || root.st === "error" || root.st === "restore_ready"
        text: root.st === "none" ? qsTr("Create vault") : root.st === "error" ? qsTr("Try again")
            : root.st === "restore_ready" ? qsTr("Unlock to finish restoring") : qsTr("Unlock")
        iconName: root.st === "none" ? "plus" : "lock-open"
        variant: "accent"
        large: true
        Tk.Flex.alignSelf: Tk.Flex.Stretch
        onClicked: vault.unlock()
    }

    Tk.Button {
        objectName: "gateRestoreToggle"
        visible: root.st === "none" || root.st === "invalidated" || root.st === "error"
        text: qsTr("Restore from backup")
        iconName: "upload"
        variant: "outline"
        Tk.Flex.alignSelf: Tk.Flex.Stretch
        onClicked: root.restoreOpen = !root.restoreOpen
    }

    Tk.Card {
        objectName: "gateRestore"
        visible: root.restoreOpen && (root.st === "none" || root.st === "invalidated" || root.st === "error")
        Tk.Flex.alignSelf: Tk.Flex.Stretch
        Tk.Flex {
            direction: Tk.Flex.Column
            gap: Tk.Theme.space.sm
            Tk.Label { text: qsTr("Backup passphrase"); Tk.Flex.alignSelf: Tk.Flex.Stretch }
            VaultSecretField {
                id: restorePass
                objectName: "restorePass"
                placeholder: qsTr("Passphrase used when the backup was made")
                Tk.Flex.alignSelf: Tk.Flex.Stretch
            }
            Tk.Button {
                objectName: "restoreChoose"
                text: qsTr("Choose backup file")
                iconName: "folder"
                variant: "accent"
                enabled: restorePass.text.length > 0
                Tk.Flex.alignSelf: Tk.Flex.Stretch
                onClicked: { vault.importBackup(restorePass.text, "restore"); restorePass.clear() }
            }
            Tk.Caption {
                objectName: "restoreStatus"
                visible: vault.backupOp === "import" && vault.backupStatus !== "none"
                text: vault.backupStatus === "wrong-passphrase" ? qsTr("Wrong passphrase, or the file is damaged.")
                    : vault.backupStatus === "failed" ? qsTr("That file could not be read as a vault backup.")
                    : vault.backupStatus === "cancelled" ? qsTr("No file chosen.")
                    : vault.backupStatus === "working" ? qsTr("Decrypting…") : ""
                muted: true
                wrapMode: Text.Wrap
                Tk.Flex.alignSelf: Tk.Flex.Stretch
            }
        }
    }

    Tk.Flex {
        visible: root.st === "invalidated" || root.st === "error"
        direction: Tk.Flex.Column
        gap: Tk.Theme.space.sm
        Tk.Flex.alignSelf: Tk.Flex.Stretch
        Tk.Button {
            objectName: "gateErase"
            visible: !root.eraseConfirm
            text: qsTr("Erase this vault and start over")
            variant: "danger"
            Tk.Flex.alignSelf: Tk.Flex.Stretch
            onClicked: root.eraseConfirm = true
        }
        Tk.Notice {
            visible: root.eraseConfirm
            variant: "danger"
            text: qsTr("This deletes the unreadable vault file for good. A backup is the only way back.")
            Tk.Flex.alignSelf: Tk.Flex.Stretch
        }
        Tk.Flex {
            visible: root.eraseConfirm
            direction: Tk.Flex.Row
            gap: Tk.Theme.space.sm
            Tk.Flex.alignSelf: Tk.Flex.Stretch
            Tk.Button { text: qsTr("Cancel"); variant: "outline"; Tk.Flex.grow: 1; onClicked: root.eraseConfirm = false }
            Tk.Button {
                objectName: "gateEraseConfirm"
                text: qsTr("Erase")
                variant: "danger"
                Tk.Flex.grow: 1
                onClicked: { root.eraseConfirm = false; vault.eraseVault() }
            }
        }
    }

    Tk.Caption {
        visible: root.st === "locked"
        text: vault.strongBox ? qsTr("Key held in the phone's StrongBox security chip.") : qsTr("Key held in the phone's secure hardware.")
        muted: true
        wrapMode: Text.Wrap
        Tk.Flex.alignSelf: Tk.Flex.Stretch
    }
}

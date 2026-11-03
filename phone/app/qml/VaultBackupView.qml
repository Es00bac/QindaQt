// SPDX-License-Identifier: LGPL-3.0-or-later
import QtQuick
import QindaTK as Tk

// Encrypted backup export and import, plus the clipboard setting. Backups are the only way vault
// content leaves the phone and they are always encrypted with a passphrase (PBKDF2, 600000+ rounds).
// AGENT-GUARD: there is deliberately no plaintext export here.
Tk.Flex {
    id: root

    signal navigate(string view, string entryId)

    property string exportError: ""
    property int importMode: 0            // 0 merge, 1 replace
    property bool importConfirm: false
    // Strength is evaluated on submit only: the passphrase is not sent to the engine per keystroke.
    property var hint: ({})
    function wipe() { expPass.clear(); expConfirm.clear(); impPass.clear() }
    Component.onDestruction: wipe()

    direction: Tk.Flex.Column
    gap: Tk.Theme.space.sm

    Tk.Button { text: qsTr("All passwords"); iconName: "chevron-left"; variant: "ghost"; onClicked: root.navigate("list", "") }
    Tk.Heading { text: qsTr("Backup"); level: 2 }

    Tk.Notice {
        objectName: "backupStatus"
        visible: vault.backupStatus !== "none" && vault.backupOp !== ""
        variant: vault.backupStatus === "done" ? "success" : (vault.backupStatus === "working" || vault.backupStatus === "picking") ? "info" : "warning"
        text: vault.backupOp === "export"
            ? (vault.backupStatus === "done" ? qsTr("Backup saved (%1 entries).").arg(vault.backupCount)
                : vault.backupStatus === "cancelled" ? qsTr("Export cancelled; nothing was written.")
                : vault.backupStatus === "failed" ? qsTr("The backup could not be written.")
                : qsTr("Encrypting the backup…"))
            : (vault.backupStatus === "done" ? qsTr("Imported %1 entries.").arg(vault.backupCount)
                : vault.backupStatus === "wrong-passphrase" ? qsTr("Wrong passphrase, or the file is damaged. Nothing was changed.")
                : vault.backupStatus === "cancelled" ? qsTr("Import cancelled; nothing was changed.")
                : vault.backupStatus === "failed" ? qsTr("That file could not be read as a vault backup. Nothing was changed.")
                : qsTr("Reading the backup…"))
        Tk.Flex.alignSelf: Tk.Flex.Stretch
    }

    Tk.SectionHeader { title: qsTr("Export encrypted backup"); Tk.Flex.alignSelf: Tk.Flex.Stretch }
    Tk.Caption {
        text: qsTr("The file is encrypted with the passphrase you choose. Without it nobody can read the backup, including you.")
        muted: true
        wrapMode: Text.Wrap
        Tk.Flex.alignSelf: Tk.Flex.Stretch
    }
    VaultSecretField { id: expPass; objectName: "exportPass"; placeholder: qsTr("Backup passphrase"); Tk.Flex.alignSelf: Tk.Flex.Stretch }
    VaultSecretField { id: expConfirm; objectName: "exportConfirm"; placeholder: qsTr("Repeat the passphrase"); Tk.Flex.alignSelf: Tk.Flex.Stretch }
    Tk.Flex {
        visible: root.hint.label !== undefined
        direction: Tk.Flex.Row
        align: Tk.Flex.Center
        gap: Tk.Theme.space.sm
        Tk.Flex.alignSelf: Tk.Flex.Stretch
        Tk.Badge {
            objectName: "strengthBadge"
            text: root.hint.label || ""
            variant: root.hint.label === "strong" || root.hint.label === "good" ? "success" : root.hint.label === "fair" ? "warning" : "danger"
        }
        Tk.Caption {
            Tk.Flex.grow: 1
            Tk.Flex.basis: 0
            text: root.hint.ok ? qsTr("About %1 bits").arg(root.hint.bits) : (root.hint.reason || "")
            muted: true
            wrapMode: Text.Wrap
        }
    }
    Tk.Caption { visible: root.exportError !== ""; text: root.exportError; wrapMode: Text.Wrap; Tk.Flex.alignSelf: Tk.Flex.Stretch }
    Tk.Button {
        objectName: "exportGo"
        text: qsTr("Choose where to save")
        iconName: "download"
        variant: "accent"
        enabled: expPass.text.length > 0 && expPass.text === expConfirm.text
        Tk.Flex.alignSelf: Tk.Flex.Stretch
        onClicked: {
            root.hint = vault.passphraseHint(expPass.text)
            if (root.hint.ok !== true) { root.exportError = root.hint.reason || qsTr("That passphrase is too weak."); return }
            root.exportError = vault.exportBackup(expPass.text, expConfirm.text)
            if (root.exportError === "") { expPass.clear(); expConfirm.clear(); root.hint = ({}) }
        }
    }

    Tk.Divider { Tk.Flex.alignSelf: Tk.Flex.Stretch }
    Tk.SectionHeader { title: qsTr("Import a backup"); Tk.Flex.alignSelf: Tk.Flex.Stretch }
    VaultSecretField { id: impPass; objectName: "importPass"; placeholder: qsTr("Passphrase of that backup"); Tk.Flex.alignSelf: Tk.Flex.Stretch }
    Tk.Segmented {
        objectName: "importMode"
        model: [qsTr("Merge"), qsTr("Replace all")]
        currentIndex: root.importMode
        Tk.Flex.alignSelf: Tk.Flex.Stretch
        onActivated: index => { root.importMode = index; root.importConfirm = false }
    }
    Tk.Caption {
        text: root.importMode === 0 ? qsTr("Merge adds new entries and keeps the newer copy of any that exist on both.")
            : qsTr("Replace deletes every entry on this phone and uses the backup instead.")
        muted: true
        wrapMode: Text.Wrap
        Tk.Flex.alignSelf: Tk.Flex.Stretch
    }
    Tk.Button {
        objectName: "importGo"
        visible: !root.importConfirm
        text: qsTr("Choose backup file")
        iconName: "upload"
        variant: "outline"
        enabled: impPass.text.length > 0
        Tk.Flex.alignSelf: Tk.Flex.Stretch
        onClicked: root.importConfirm = true
    }
    Tk.Notice {
        objectName: "importConfirmNotice"
        visible: root.importConfirm
        variant: root.importMode === 1 ? "danger" : "warning"
        text: root.importMode === 1 ? qsTr("Replace all %1 entries on this phone with the backup?").arg(vault.entries.length)
            : qsTr("Merge the backup into this vault?")
        Tk.Flex.alignSelf: Tk.Flex.Stretch
    }
    Tk.Flex {
        visible: root.importConfirm
        direction: Tk.Flex.Row
        gap: Tk.Theme.space.sm
        Tk.Flex.alignSelf: Tk.Flex.Stretch
        Tk.Button { text: qsTr("Cancel"); variant: "outline"; Tk.Flex.grow: 1; onClicked: root.importConfirm = false }
        Tk.Button {
            objectName: "importConfirm"
            text: root.importMode === 1 ? qsTr("Replace") : qsTr("Merge")
            variant: root.importMode === 1 ? "danger" : "accent"
            Tk.Flex.grow: 1
            onClicked: {
                root.importConfirm = false
                vault.importBackup(impPass.text, root.importMode === 1 ? "replace" : "merge")
                impPass.clear()
            }
        }
    }

    Tk.Divider { Tk.Flex.alignSelf: Tk.Flex.Stretch }
    Tk.SectionHeader { title: qsTr("Settings"); Tk.Flex.alignSelf: Tk.Flex.Stretch }
    Tk.Switch {
        objectName: "copySwitch"
        text: qsTr("Allow copying passwords")
        checked: vault.copyEnabled
        Tk.Flex.alignSelf: Tk.Flex.Stretch
        onToggled: vault.setCopyEnabled(checked)
    }
    Tk.Caption {
        text: qsTr("Off by default. When on, Copy marks the clipboard sensitive and clears it after 30 seconds.")
        muted: true
        wrapMode: Text.Wrap
        Tk.Flex.alignSelf: Tk.Flex.Stretch
    }
    Tk.Button { text: qsTr("Clear this message"); variant: "ghost"; visible: vault.backupStatus !== "none"; onClicked: vault.clearBackupStatus() }
}

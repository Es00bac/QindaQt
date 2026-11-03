// SPDX-License-Identifier: LGPL-3.0-or-later
import QtQuick
import QindaTK as Tk

// One entry. The password and notes are requested from the engine only on Show, held in `secret` for
// at most 10 s, and dropped on Hide, on timeout and when this view is destroyed.
// AGENT-GUARD: never copy `secret` into a property of another object or a binding that outlives it.
Tk.Flex {
    id: root

    property string entryId: ""
    signal navigate(string view, string entryId)

    property var info: vault.entry(entryId)
    property var secret: null          // {password, notes} while revealed, else null
    property bool confirmDelete: false
    property bool copied: false

    function hide() { secret = null }
    Component.onDestruction: secret = null

    direction: Tk.Flex.Column
    gap: Tk.Theme.space.md

    Tk.Button { text: qsTr("All passwords"); iconName: "chevron-left"; variant: "ghost"; onClicked: root.navigate("list", "") }

    Tk.Heading { objectName: "detailTitle"; text: root.info.title || ""; level: 2; wrapMode: Text.Wrap; Tk.Flex.alignSelf: Tk.Flex.Stretch }

    Tk.Card {
        Tk.Flex.alignSelf: Tk.Flex.Stretch
        Tk.Flex {
            direction: Tk.Flex.Column
            gap: Tk.Theme.space.sm
            Tk.Caption { text: qsTr("Username"); muted: true }
            Tk.Label { objectName: "detailUser"; text: root.info.username || qsTr("(none)"); wrapMode: Text.Wrap; Tk.Flex.alignSelf: Tk.Flex.Stretch }
            Tk.Caption { text: qsTr("Website"); muted: true }
            Tk.Label { objectName: "detailUrl"; text: root.info.url || qsTr("(none)"); wrapMode: Text.Wrap; Tk.Flex.alignSelf: Tk.Flex.Stretch }
            Tk.Caption { text: qsTr("Password"); muted: true }
            Tk.Flex {
                direction: Tk.Flex.Row
                align: Tk.Flex.Center
                gap: Tk.Theme.space.sm
                Tk.Flex.alignSelf: Tk.Flex.Stretch
                Tk.Label {
                    objectName: "detailPassword"
                    Tk.Flex.grow: 1
                    Tk.Flex.basis: 0
                    mono: true
                    wrapMode: Text.WrapAnywhere
                    text: root.secret ? root.secret.password : "••••••••••••"
                }
                Tk.IconButton {
                    objectName: "detailReveal"
                    iconName: root.secret ? "eye-off" : "eye"
                    tooltip: root.secret ? qsTr("Hide password") : qsTr("Show password for 10 seconds")
                    onClicked: root.secret = root.secret ? null : vault.reveal(root.entryId)
                }
                Tk.IconButton {
                    objectName: "detailCopy"
                    visible: vault.copyEnabled
                    iconName: "copy"
                    tooltip: qsTr("Copy password (cleared after 30 seconds)")
                    onClicked: root.copied = vault.copyPassword(root.entryId)
                }
            }
            Tk.Caption { visible: root.copied; text: qsTr("Copied. The clipboard clears in 30 seconds."); muted: true }
            Tk.Caption { visible: root.secret !== null && (root.secret.notes || "") !== ""; text: qsTr("Notes"); muted: true }
            Tk.Label {
                objectName: "detailNotes"
                visible: root.secret !== null && (root.secret.notes || "") !== ""
                text: root.secret ? root.secret.notes : ""
                wrapMode: Text.Wrap
                Tk.Flex.alignSelf: Tk.Flex.Stretch
            }
        }
    }

    Timer {
        interval: 10000
        running: root.secret !== null
        onTriggered: root.hide()
    }

    VaultTypeSheet { id: typeSheet; objectName: "typeSheet"; entryId: root.entryId; onClosed: root.hide() }
    Tk.Flex {
        visible: !typeSheet.busy
        direction: Tk.Flex.Column
        gap: Tk.Theme.space.sm
        Tk.Flex.alignSelf: Tk.Flex.Stretch
        Tk.Caption { text: qsTr("Type on desktop"); muted: true }
        Tk.Button { objectName: "typeUser"; text: qsTr("Type username"); iconName: "monitor"; variant: "outline"; Tk.Flex.alignSelf: Tk.Flex.Stretch
            onClicked: { root.hide(); typeSheet.begin("username") } }
        Tk.Button { objectName: "typePassword"; text: qsTr("Type password"); iconName: "monitor"; variant: "outline"; Tk.Flex.alignSelf: Tk.Flex.Stretch
            onClicked: { root.hide(); typeSheet.begin("password") } }
        Tk.Button { objectName: "typeAutotype"; text: qsTr("Type username + password (autotype)"); iconName: "monitor"; variant: "outline"; Tk.Flex.alignSelf: Tk.Flex.Stretch
            onClicked: { root.hide(); typeSheet.begin("autotype") } }
    }
    Tk.Flex {
        direction: Tk.Flex.Row
        gap: Tk.Theme.space.sm
        Tk.Flex.alignSelf: Tk.Flex.Stretch
        Tk.Button { objectName: "detailEdit"; text: qsTr("Edit"); iconName: "pencil"; variant: "outline"; Tk.Flex.grow: 1
            onClicked: { root.hide(); root.navigate("edit", root.entryId) } }
        Tk.Button { objectName: "detailDelete"; text: qsTr("Delete"); iconName: "trash"; variant: "danger"; Tk.Flex.grow: 1
            visible: !root.confirmDelete; onClicked: root.confirmDelete = true }
    }
    Tk.Notice {
        visible: root.confirmDelete
        variant: "danger"
        text: qsTr("Delete this password for good? Backups keep their own copy.")
        Tk.Flex.alignSelf: Tk.Flex.Stretch
    }
    Tk.Flex {
        visible: root.confirmDelete
        direction: Tk.Flex.Row
        gap: Tk.Theme.space.sm
        Tk.Flex.alignSelf: Tk.Flex.Stretch
        Tk.Button { text: qsTr("Keep it"); variant: "outline"; Tk.Flex.grow: 1; onClicked: root.confirmDelete = false }
        Tk.Button {
            objectName: "detailDeleteConfirm"
            text: qsTr("Delete")
            variant: "danger"
            Tk.Flex.grow: 1
            onClicked: { vault.remove(root.entryId); root.navigate("list", "") }
        }
    }
}

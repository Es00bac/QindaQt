// SPDX-License-Identifier: LGPL-3.0-or-later
import QtQuick
import QindaTK as Tk

// Add or edit an entry. An empty `entryId` is a new entry; `withGenerator` opens the generator.
// Secrets are loaded into the fields on open and wiped when this view is destroyed.
Tk.Flex {
    id: root

    property string entryId: ""
    property bool withGenerator: false
    property string error: ""
    signal navigate(string view, string entryId)

    function load() {
        if (entryId === "") return
        const e = vault.entry(entryId)
        titleField.text = e.title || ""
        userField.text = e.username || ""
        urlField.text = e.url || ""
        autotypeField.text = e.autotype || ""
        const s = vault.reveal(entryId)
        passField.text = s.password || ""
        notesField.text = s.notes || ""
    }
    function wipe() { passField.clear(); notesField.text = "" }
    Component.onCompleted: load()
    Component.onDestruction: wipe()

    direction: Tk.Flex.Column
    gap: Tk.Theme.space.sm

    Tk.Button { text: qsTr("Cancel"); iconName: "x"; variant: "ghost"
        onClicked: root.navigate(root.entryId === "" ? "list" : "detail", root.entryId) }
    Tk.Heading { text: root.entryId === "" ? qsTr("New password") : qsTr("Edit password"); level: 2 }

    Tk.Label { text: qsTr("Title") }
    Tk.TextField { id: titleField; objectName: "editTitle"; placeholderText: qsTr("Example Mail"); Tk.Flex.alignSelf: Tk.Flex.Stretch
        inputMethodHints: Qt.ImhNoPredictiveText }
    Tk.Label { text: qsTr("Username") }
    Tk.TextField { id: userField; objectName: "editUser"; Tk.Flex.alignSelf: Tk.Flex.Stretch
        inputMethodHints: Qt.ImhSensitiveData | Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase | Qt.ImhEmailCharactersOnly }
    Tk.Label { text: qsTr("Website") }
    Tk.TextField { id: urlField; objectName: "editUrl"; placeholderText: qsTr("https://"); Tk.Flex.alignSelf: Tk.Flex.Stretch
        inputMethodHints: Qt.ImhUrlCharactersOnly | Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase }
    Tk.Label { text: qsTr("Password") }
    VaultSecretField { id: passField; objectName: "editPassword"; Tk.Flex.alignSelf: Tk.Flex.Stretch }
    Tk.Button {
        objectName: "editGenerate"
        text: genPanel.visible ? qsTr("Hide generator") : qsTr("Generate a password")
        iconName: "refresh-cw"
        variant: "outline"
        Tk.Flex.alignSelf: Tk.Flex.Stretch
        onClicked: root.withGenerator = !root.withGenerator
    }
    VaultGeneratorPanel {
        id: genPanel
        objectName: "editGeneratorPanel"
        visible: root.withGenerator
        Tk.Flex.alignSelf: Tk.Flex.Stretch
        onUse: v => { passField.text = v; root.withGenerator = false }
    }
    Tk.Label { text: qsTr("Notes") }
    Tk.TextArea { id: notesField; objectName: "editNotes"; rows: 3; Tk.Flex.alignSelf: Tk.Flex.Stretch
        inputMethodHints: Qt.ImhSensitiveData | Qt.ImhNoPredictiveText }
    Tk.Label { text: qsTr("Autotype sequence") }
    Tk.TextField {
        id: autotypeField
        objectName: "editAutotype"
        mono: true
        placeholderText: "{USERNAME}{TAB}{PASSWORD}{ENTER}"
        Tk.Flex.alignSelf: Tk.Flex.Stretch
        inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
    }
    Tk.Caption {
        text: qsTr("Used later when typing into the desktop. Tokens: {USERNAME} {PASSWORD} {URL} {TAB} {ENTER} {KEY name} {DELAY ms}.")
        muted: true
        wrapMode: Text.Wrap
        Tk.Flex.alignSelf: Tk.Flex.Stretch
    }

    Tk.Notice { objectName: "editError"; visible: root.error !== ""; variant: "danger"; text: root.error; Tk.Flex.alignSelf: Tk.Flex.Stretch }
    Tk.Button {
        objectName: "editSave"
        text: qsTr("Save")
        iconName: "save"
        variant: "accent"
        large: true
        Tk.Flex.alignSelf: Tk.Flex.Stretch
        onClicked: {
            const r = vault.save({ id: root.entryId, title: titleField.text, username: userField.text, url: urlField.text,
                                   password: passField.text, notes: notesField.text, autotype: autotypeField.text })
            if (r.indexOf("ok:") === 0) { root.wipe(); root.navigate("detail", r.substring(3)) }
            else root.error = r.substring(4)
        }
    }
}

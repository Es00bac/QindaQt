// SPDX-License-Identifier: LGPL-3.0-or-later
import QtQuick
import QindaTK as Tk

// The unlocked entry list: search over title, username and URL (never secrets), plus the actions.
Tk.Flex {
    id: root

    signal navigate(string view, string entryId)

    property string query: ""
    // Filtered and sorted copy of vault.entries; recomputed when either input changes.
    readonly property var shown: {
        const q = query.trim().toLowerCase()
        const all = vault.entries.slice()
        all.sort((a, b) => a.title.toLowerCase() < b.title.toLowerCase() ? -1 : 1)
        if (q === "") return all
        return all.filter(e => e.title.toLowerCase().indexOf(q) >= 0 || e.username.toLowerCase().indexOf(q) >= 0
            || e.url.toLowerCase().indexOf(q) >= 0)
    }

    direction: Tk.Flex.Column
    gap: Tk.Theme.space.sm

    Tk.SearchField {
        objectName: "vaultSearch"
        placeholderText: qsTr("Search title, user or site")
        Tk.Flex.alignSelf: Tk.Flex.Stretch
        onSearched: text => root.query = text
        // The keyboard must not learn what is searched either.
        inputMethodHints: Qt.ImhSensitiveData | Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
    }

    Tk.Flex {
        direction: Tk.Flex.Row
        gap: Tk.Theme.space.sm
        Tk.Flex.alignSelf: Tk.Flex.Stretch
        Tk.Button { objectName: "vaultAdd"; text: qsTr("Add"); iconName: "plus"; variant: "accent"; Tk.Flex.grow: 1
            onClicked: root.navigate("edit", "") }
        Tk.Button { objectName: "vaultGenerate"; text: qsTr("Generate"); iconName: "refresh-cw"; variant: "outline"; Tk.Flex.grow: 1
            onClicked: root.navigate("generate", "") }
    }
    Tk.Flex {
        direction: Tk.Flex.Row
        gap: Tk.Theme.space.sm
        Tk.Flex.alignSelf: Tk.Flex.Stretch
        Tk.Button { objectName: "vaultBackup"; text: qsTr("Backup"); iconName: "archive"; variant: "outline"; Tk.Flex.grow: 1
            onClicked: root.navigate("backup", "") }
        Tk.Button { objectName: "vaultLock"; text: qsTr("Lock now"); iconName: "lock"; variant: "outline"; Tk.Flex.grow: 1
            onClicked: vault.lock() }
    }

    Tk.EmptyState {
        objectName: "vaultEmpty"
        visible: vault.entries.length === 0
        iconName: "key-round"
        title: qsTr("No passwords yet")
        text: qsTr("Add one, or restore a backup. Everything stays encrypted on this phone.")
        Tk.Flex.alignSelf: Tk.Flex.Stretch
    }
    Tk.Caption {
        visible: vault.entries.length > 0 && root.shown.length === 0
        text: qsTr("Nothing matches.")
        muted: true
    }

    Tk.Flex {
        objectName: "vaultRows"
        direction: Tk.Flex.Column
        Tk.Flex.alignSelf: Tk.Flex.Stretch
        Repeater {
            model: root.shown
            delegate: Tk.ListRow {
                required property var modelData
                objectName: "vaultRow_" + modelData.id
                text: modelData.title
                secondaryText: modelData.username
                iconName: "key"
                Tk.Flex.alignSelf: Tk.Flex.Stretch
                onClicked: root.navigate("detail", modelData.id)
            }
        }
    }
}
